#include "ResizeForm.h"
#include <LWSUI/UIHost.hpp>
#include <LWSUI/Containers.hpp>
#include <LWSUI/Primitives.hpp>
#include <LWSUI/Composites.hpp>
#include <LWSUI/Choice.hpp>
#include <LWSUI/Theme.hpp>
#include <LWS/Window.hpp>
#include <LWS/WindowTypes.hpp>
#include <algorithm>
#include <charconv>
#include <cmath>
#include <fstream>
#include <format>
#include <sstream>
#include <string_view>
#include <tuple>
#include <vector>

namespace OIV
{
    namespace
    {
        constexpr int32_t DialogWidth            = 760;
        constexpr int32_t DialogHeightWithDpi    = 540;
        constexpr int32_t DialogHeightWithoutDpi = 500;
        LWS::LogicalSize DialogSize(bool showDpi)
        {
            return {DialogWidth, showDpi ? DialogHeightWithDpi : DialogHeightWithoutDpi};
        }
        std::string FormatNumber(double value)
        {
            auto result = FormatResizeDimension(value);
            while (result.back() == '0')
                result.pop_back();
            if (result.back() == '.')
                result.pop_back();
            return result;
        }
        std::string FormatPixels(double width, double height)
        {
            return std::format("{:.0f} × {:.0f} px", width, height);
        }
        std::unique_ptr<LWSUI::Label> Label(std::string text,
                                            LWSUI::Control::FontRole role = LWSUI::Control::FontRole::Normal)
        {
            auto result      = std::make_unique<LWSUI::Label>(std::move(text));
            result->fontRole = role;
            return result;
        }
        LWSUI::Theme CompactTheme()
        {
            auto theme               = LWSUI::MakeTheme(LWSUI::ThemePreset::Light);
            theme.font.size          = 12;
            theme.smallFont.size     = 10;
            theme.headingFont.size   = 14;
            theme.headingFont.weight = 600;
            theme.titleFont.size     = 16;
            theme.titleFont.weight   = 700;
            theme.rowHeight          = 21;
            theme.padding            = 4;
            theme.labelPadding       = 1;
            theme.textPadding        = 2;
            theme.radioHeight        = 22;
            theme.spacing            = 3;
            theme.choiceGap          = 6;
            theme.checkboxSize       = 14;
            return theme;
        }
        LWSUI::Theme WithBackground(LWSUI::Theme style, LLUtils::Color color)
        {
            style.background = color;
            return style;
        }

        struct StandardResolution
        {
            std::string designation;
            uint32_t width{}, height{};

            uint64_t Area() const { return static_cast<uint64_t>(width) * height; }
        };

        std::vector<LWSUI::Choice> StandardResolutionChoices(const std::filesystem::path& resources)
        {
            std::vector<StandardResolution> resolutions;
            std::ifstream file(resources / "StandardResolutions.tsv");
            std::string line;
            while (std::getline(file, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.empty() || line.front() == '#')
                    continue;
                std::istringstream row(line);
                StandardResolution resolution;
                std::string width, height;
                if (std::getline(row, resolution.designation, '\t') && std::getline(row, width, '\t') &&
                    std::getline(row, height))
                {
                    const auto parsedWidth  = std::from_chars(width.data(), width.data() + width.size(),
                                                              resolution.width);
                    const auto parsedHeight = std::from_chars(height.data(), height.data() + height.size(),
                                                              resolution.height);
                    if (parsedWidth.ec == std::errc{} && parsedWidth.ptr == width.data() + width.size() &&
                        parsedHeight.ec == std::errc{} && parsedHeight.ptr == height.data() + height.size() &&
                        resolution.width && resolution.height)
                        resolutions.push_back(std::move(resolution));
                }
            }

            if (resolutions.empty())
                resolutions = {{"VGA", 640, 480},      {"SVGA", 800, 600},      {"XGA", 1024, 768},
                               {"HD", 1280, 720},      {"Full HD", 1920, 1080}, {"QHD", 2560, 1440},
                               {"4K UHD", 3840, 2160}, {"8K UHD", 7680, 4320}};
            std::ranges::sort(
                resolutions,
                [](const auto& left, const auto& right)
                {
                    return std::tuple{left.Area(), left.width, left.height, std::string_view{left.designation}} >
                           std::tuple{right.Area(), right.width, right.height, std::string_view{right.designation}};
                });

            std::vector<LWSUI::Choice> choices;
            choices.reserve(resolutions.size() + 2);
            choices.push_back({"Current", "current"});
            for (const auto& resolution : resolutions)
            {
                auto label = resolution.designation.empty() || resolution.designation == "N/A"
                                 ? std::string{}
                                 : resolution.designation + " — ";
                label += std::to_string(resolution.width) + " × " + std::to_string(resolution.height) + " px";
                auto value = std::to_string(resolution.width) + "x" + std::to_string(resolution.height);
                choices.push_back({std::move(label), std::move(value)});
            }
            choices.push_back({"Custom…", "custom"});
            return choices;
        }
    }  // namespace

    // These compact rows use the label's actual width instead of distributing spare width between choices.
    class CompactRadioGroup final : public LWSUI::RadioGroup
    {
      public:

        using RadioGroup::RadioGroup;

      protected:

        LWSUI::Size OnMeasure(LWSUI::Size available) override
        {
            LWSUI::Size result{};
            for (const auto& child : Children())
            {
                const auto& button = static_cast<const LWSUI::RadioButton&>(*child);
                const auto width   = Host()->MeasureText(button.Text(), 100000, false, button.Font()).width +
                                   Style().checkboxSize + Style().choiceGap / 2;
                const auto size = child->Measure({width, available.height});
                result.width += size.width;
                result.height = std::max(result.height, size.height);
            }
            if (!Children().empty())
                result.width += OptionGap * (Children().size() - 1);
            return result;
        }
        void OnArrange() override
        {
            const auto bounds = Bounds();
            float x           = bounds.x;
            for (const auto& child : Children())
            {
                const auto width = child->DesiredSize().width;
                child->Arrange({x, bounds.y, width, bounds.height});
                x += width + OptionGap;
            }
        }

      private:

        static constexpr float OptionGap = 12;
    };

    // Both panes follow the settings' natural height; the preview fills the same row without enlarging it.
    class ResizeContent final : public LWSUI::Container
    {
      protected:

        LWSUI::Size OnMeasure(LWSUI::Size available) override
        {
            const auto left = Children()[0]->Measure(
                {std::max(0.f, available.width - PreviewWidth - Gap), available.height});
            Children()[1]->Measure({PreviewWidth, left.height});
            return {available.width, left.height};
        }
        void OnArrange() override
        {
            const auto b      = Bounds();
            const auto height = std::min(b.height, Children()[0]->DesiredSize().height);
            const auto width  = std::max(0.f, b.width - PreviewWidth - Gap);
            Children()[0]->Arrange({b.x, b.y, width, height});
            Children()[1]->Arrange({b.x + width + Gap, b.y, PreviewWidth, height});
        }

      private:

        static constexpr float PreviewWidth = 400;
        static constexpr float Gap          = 12;
    };

    // The borderless window moves from blank content while controls keep their own hit targets.
    class DraggableSurface final : public LWSUI::Panel
    {
      public:

        explicit DraggableSurface(std::unique_ptr<LWSUI::Control> content) : LWSUI::Panel(std::move(content)) {}

      protected:

        LWSUI::Control* HitTest(float x, float y) override
        {
            auto* target = Panel::HitTest(x, y);
            if (target && target != this && !dynamic_cast<LWSUI::Button*>(target) &&
                !dynamic_cast<LWSUI::TextBox*>(target) && !dynamic_cast<LWSUI::RadioGroup*>(target) &&
                !dynamic_cast<LWSUI::ComboBox*>(target) && !dynamic_cast<LWSUI::Thumbwheel*>(target))
                target = this;
            return target;
        }

        bool OnInput(const LWSUI::Input& input) override
        {
            return input.kind == LWSUI::InputKind::Down && input.button == LWS::MouseButton::Left && Host() &&
                   Host()->Window().BeginWindowDrag(LWS::WindowDragOperation::Move) == LWS::Result::Success;
        }
    };

    // The supplied image replaces the colorful mountain illustration when available.
    class ResizePreview final : public LWSUI::Control
    {
      public:

        void SetBitmap(LWS::BitmapSharedPtr bitmap, double sourceAspect)
        {
            bitmap_ = std::move(bitmap);
            if (std::isfinite(sourceAspect) && sourceAspect > 0)
                sourceAspect_ = sourceAspect;
            Invalidate();
        }
        void SetBlackBars(bool value)
        {
            blackBars_ = value;
            Invalidate();
        }
        void SetAspect(double value)
        {
            if (std::isfinite(value) && value > 0)
                aspect_ = value;
            Invalidate();
        }

      protected:

        LWSUI::Size OnMeasure(LWSUI::Size available) override
        {
            return {available.width, std::min(available.height, 240.f)};
        }
        void OnRender(LWSUI::Canvas& canvas) override
        {
            const auto b = Bounds();
            canvas.Fill(b.x, b.y, b.width, b.height, LLUtils::Color(229, 234, 241));
            constexpr float inset = 10;
            const float aspect    = static_cast<float>(aspect_);
            const float width     = std::min(b.width - 2 * inset, (b.height - 2 * inset) * aspect);
            const float height    = width / aspect;
            const float frameX = b.x + (b.width - width) / 2, frameY = b.y + (b.height - height) / 2;
            if (blackBars_)
                canvas.Fill(frameX, frameY, width, height, LLUtils::Color(18, 24, 34));
            float imageWidth = width, imageHeight = height;
            if (blackBars_ && sourceAspect_ > 0)
            {
                if (sourceAspect_ > aspect)
                    imageHeight = width / static_cast<float>(sourceAspect_);
                else
                    imageWidth = height * static_cast<float>(sourceAspect_);
            }
            const float imageX = frameX + (width - imageWidth) / 2;
            const float imageY = frameY + (height - imageHeight) / 2;
            if (bitmap_)
                canvas.Image(bitmap_->GetBuffer(), imageX, imageY, imageWidth, imageHeight);
            else
            {
                canvas.Fill(imageX, imageY, imageWidth, imageHeight, LLUtils::Color(128, 185, 217));
                canvas.FillCircle(imageX + imageWidth * .84f, imageY + imageHeight * .20f,
                                  std::min(imageWidth, imageHeight) * .09f, LLUtils::Color(255, 227, 166));
                // Horizontal strips give the mountains crisp triangular silhouettes without thick diagonal strokes.
                const auto mountain =
                    [&](float left, float peak, float top, float right, float bottom, LLUtils::Color color)
                {
                    for (float y = top; y < bottom; y += 1.f)
                    {
                        const float fraction = (y - top) / (bottom - top);
                        const float x1       = peak + (left - peak) * fraction;
                        const float x2       = peak + (right - peak) * fraction;
                        canvas.Fill(imageX + x1 * imageWidth, imageY + y, (x2 - x1) * imageWidth,
                                    std::min(1.f, bottom - y), color);
                    }
                };
                mountain(.0f, .27f, imageHeight * .35f, .65f, imageHeight * .90f, LLUtils::Color(56, 124, 160));
                mountain(.30f, .62f, imageHeight * .42f, 1.f, imageHeight * .90f, LLUtils::Color(56, 124, 160));
                mountain(.04f, .38f, imageHeight * .42f, .78f, imageHeight, LLUtils::Color(37, 95, 130));
                mountain(.44f, .74f, imageHeight * .56f, 1.f, imageHeight, LLUtils::Color(37, 95, 130));
                canvas.Fill(imageX, imageY + imageHeight * .85f, imageWidth, imageHeight * .15f,
                            LLUtils::Color(78, 157, 114));
            }
        }

      private:

        double aspect_       = 400.0 / 304.0;
        double sourceAspect_ = 400.0 / 304.0;
        LWS::BitmapSharedPtr bitmap_;
        bool blackBars_ = false;
    };

    class FooterPanel final : public LWSUI::Panel
    {
      protected:

        void OnRender(LWSUI::Canvas& canvas) override
        {
            const auto bounds = Bounds();
            Panel::OnRender(canvas);
            canvas.Line(bounds.x, bounds.y, bounds.x + bounds.width, bounds.y, 1, LLUtils::Color(233, 237, 243));
        }
    };

    ResizeFormController::ResizeFormController(LWS::PlatformContext& platform, const std::filesystem::path& resources,
                                               ResizeFormInput input)
        : fPlatform(platform), fResources(resources), fInput(std::move(input))
    {
        fCurrentSizeMode = fInput.defaults.sizeMode;
        fCurrentUnits    = fInput.defaults.units;
        fManualFilter    = fInput.defaults.filter;
        fDPI             = std::isfinite(fInput.defaults.dpi) && fInput.defaults.dpi > 0 ? fInput.defaults.dpi : 96.0;
        if (!fInput.originalBounds.IsEmpty())
        {
            fOriginalWidth  = fInput.originalBounds.GetWidth();
            fOriginalHeight = fInput.originalBounds.GetHeight();
        }
        else if (fInput.sourceBitmap)
        {
            const auto bitmap = fInput.sourceBitmap->GetBuffer();
            if (bitmap.width && bitmap.height)
            {
                fOriginalWidth  = bitmap.width;
                fOriginalHeight = bitmap.height;
            }
        }

        fSizeBasis.mode  = fCurrentSizeMode;
        fSizeBasis.units = fCurrentUnits;
        if (fInput.defaults.size && std::isfinite(fInput.defaults.size->width) &&
            std::isfinite(fInput.defaults.size->height) && fInput.defaults.size->width > 0 &&
            fInput.defaults.size->height > 0)
        {
            fSizeBasis.width  = fInput.defaults.size->width;
            fSizeBasis.height = fInput.defaults.size->height;
        }
        else if (fCurrentSizeMode == ResizeSizeMode::Relative)
        {
            fSizeBasis.width = fSizeBasis.height = 100;
        }
        else
        {
            const double scale = SizeBasis::PixelsPerUnit(fCurrentUnits, fDPI);
            fSizeBasis.width   = fOriginalWidth / scale;
            fSizeBasis.height  = fOriginalHeight / scale;
        }
        fInitialBasis  = fSizeBasis;
        fInitialDPI    = fDPI;
        fInitialPixels = fSizeBasis.Pixels({fOriginalWidth, fOriginalHeight}, fDPI);
    }
    ResizeFormController::~ResizeFormController()
    {
        fUIHost.reset();
        fConnections.Clear();
        fDialogWindow.reset();
    }
    void ResizeFormController::Show(LWS::Window* owner)
    {
        if (!fDialogWindow)
        {
            LWS::WindowConfig config;
            config.owner           = owner;
            config.modal           = owner != nullptr;
            config.title           = LWSUI::NativeText("Resize image");
            config.clientSize      = DialogSize(fCurrentSizeMode == ResizeSizeMode::Absolute &&
                                                fCurrentUnits != ResizeUnits::Pixels);
            config.styles          = LWS::WindowStyle::NoStyle;
            config.backgroundColor = LLUtils::Color(244, 246, 248);
            fDialogWindow          = std::make_unique<LWS::Window>(fPlatform);
            if (fDialogWindow->Create(config) != LWS::Result::Success)
            {
                fDialogWindow.reset();
                return;
            }
            auto listener = fDialogWindow->Listen(
                [this](const LWS::AnyEvent& event)
                {
                    if (std::holds_alternative<LWS::EventCloseRequested>(event))
                    {
                        Dismiss();
                        return LWS::EventResponse::Handled;
                    }
                    return LWS::EventResponse::Unhandled;
                });
            if (listener)
                fWindowEvents = std::move(*listener);
            if (fInput.iconBitmap)
            {
                if (auto icon = LWS::WindowIcon::FromBitmap(fInput.iconBitmap->GetBuffer()))
                    std::ignore = fDialogWindow->SetWindowIcon(std::move(*icon));
            }
        }
        if (!fUIHost)
        {
            fUIHost                  = std::make_unique<LWSUI::UIHost>(*fDialogWindow, CompactTheme());
            auto popupStyle          = CompactTheme();
            popupStyle.spacing       = 0;
            popupStyle.menuRowHeight = 22;
            popupStyle.menuPadding   = 2;
            popupStyle.background = popupStyle.line = popupStyle.surface;
            fUIHost->SetPopupStyle(std::move(popupStyle));
            fUIHost->SetRedrawOnResize(true);
            fConnections.Connect(fUIHost->OnCommandInput,
                                 [this](const LWSUI::Input& input, bool& handled)
                                 {
                                     if (input.key == LWS::KeyCode::Escape && !input.repeat && !input.control &&
                                         !input.shift && !input.alt)
                                     {
                                         handled = true;
                                         Dismiss();
                                     }
                                 });
            BuildUI();
        }
        std::ignore = fDialogWindow->SetVisible(true);
        std::ignore = fDialogWindow->RequestShowState(LWS::WindowShowState::Restored);
        if (owner && fUIHost->Root() && fUIHost->Root()->Host())
        {
            const auto placement = owner->GetPlacement();
            if (placement.position)
            {
                const auto size = fDialogSize;
                const int w = size.x, h = size.y;
                std::ignore = fDialogWindow->RequestPlacement(
                    {.position   = LWS::Point{placement.position->x + (placement.clientSize.x - w) / 2,
                                            placement.position->y + (placement.clientSize.y - h) / 2},
                     .clientSize = size});
            }
        }
    }

    void ResizeFormController::BuildUI()
    {
        const auto& theme = fUIHost->Style();
        auto root         = std::make_unique<LWSUI::DockPanel>();
        auto header       = std::make_unique<LWSUI::Grid>();
        header->columns   = {36, -1, 32};
        header->spacing   = 6;
        auto icon         = std::make_unique<LWSUI::Image>();
        icon->size        = 32;
        icon->SetBitmap(fInput.iconBitmap);
        header->Add(std::move(icon));
        auto title     = std::make_unique<LWSUI::StackPanel>();
        title->spacing = 0;
        auto heading   = Label("Resize image", LWSUI::Control::FontRole::Title);
        heading->SetForeground(LLUtils::Color(21, 35, 59));
        title->Add(std::move(heading));
        header->Add(std::move(title));
        auto close           = std::make_unique<LWSUI::Button>("×");
        close->sizeToContent = true;
        close->SetStyle(WithBackground(theme, LLUtils::Color(245, 247, 250)));
        fConnections.Connect(close->OnClick, [this] { Dismiss(); });
        header->Add(std::move(close));
        root->AddDocked(std::move(header), LWSUI::Dock::Top, 38);

        auto content    = std::make_unique<ResizeContent>();
        auto left       = std::make_unique<LWSUI::StackPanel>();
        left->spacing   = 3;
        auto addHeading = [&](const char* text)
        {
            auto label = Label(text, LWSUI::Control::FontRole::Heading);
            label->SetForeground(LLUtils::Color(91, 108, 132));
            left->Add(std::move(label));
        };
        addHeading("Size mode");
        fSizeModeRadio = new CompactRadioGroup({{"Absolute", "absolute"}, {"Relative", "relative"}});
        fSizeModeRadio->SetValue(fCurrentSizeMode == ResizeSizeMode::Absolute ? "absolute" : "relative");
        fSizeModeRadio->orientation = LWSUI::Orientation::Horizontal;
        fConnections.Connect(fSizeModeRadio->OnChange, [this](const std::string& v) { OnSizeModeChanged(v); });
        auto modePanel = std::make_unique<LWSUI::Panel>();
        modePanel->SetCornerRadius(9);
        modePanel->padding = {8, 2, 8, 2};
        modePanel->SetStyle(WithBackground(theme, LLUtils::Color(241, 243, 247)));
        modePanel->SetContent(std::unique_ptr<LWSUI::Control>(fSizeModeRadio));
        left->Add(std::move(modePanel));

        auto preset      = std::make_unique<LWSUI::Grid>();
        preset->columns  = {-1};
        preset->spacing  = 2;
        auto presetTitle = Label("Standard resolution preset", LWSUI::Control::FontRole::Heading);
        presetTitle->SetForeground(LLUtils::Color(91, 108, 132));
        preset->Add(std::move(presetTitle));
        fStandardResolutions = new LWSUI::ComboBox(StandardResolutionChoices(fResources));
        fStandardResolutions->SetValue("current");
        fConnections.Connect(fStandardResolutions->OnChange,
                             [this](const std::string& v) { OnStandardResolutionChanged(v); });
        preset->Add(std::unique_ptr<LWSUI::Control>(fStandardResolutions));
        left->Add(std::move(preset));

        auto unitsTitle = Label("Units", LWSUI::Control::FontRole::Heading);
        fUnitsTitle     = unitsTitle.get();
        fUnitsTitle->SetForeground(LLUtils::Color(91, 108, 132));
        left->Add(std::move(unitsTitle));
        fUnitsRadio = new CompactRadioGroup(
            {{"Pixels", "pixels"}, {"Inches", "inches"}, {"Centimeters", "centimeters"}});
        fUnitsRadio->SetValue(fCurrentUnits == ResizeUnits::Pixels   ? "pixels"
                              : fCurrentUnits == ResizeUnits::Inches ? "inches"
                                                                     : "centimeters");
        fUnitsRadio->orientation = LWSUI::Orientation::Horizontal;
        fConnections.Connect(fUnitsRadio->OnChange, [this](const std::string& v) { OnUnitsChanged(v); });
        auto unitsStack     = std::make_unique<LWSUI::StackPanel>();
        unitsStack->spacing = 2;
        unitsStack->Add(std::unique_ptr<LWSUI::Control>(fUnitsRadio));
        auto dpiBox             = std::make_unique<LWSUI::Panel>();
        fDPIBox                 = dpiBox.get();
        dpiBox->padding         = {0, 1, 0, 0};
        dpiBox->paintBackground = false;
        auto dpiRow             = std::make_unique<LWSUI::Grid>();
        dpiRow->columns         = {30, 72, 54};
        dpiRow->spacing         = 4;
        auto dpiName            = Label("DPI", LWSUI::Control::FontRole::Small);
        dpiName->SetForeground(LLUtils::Color(133, 145, 164));
        dpiRow->Add(std::move(dpiName));
        fDPIInput = new LWSUI::TextBox();
        fDPIInput->SetInputFilter(LWSUI::InputFilters::Number(false, true));
        fDPIInput->SetFontWeight(600);
        fDPIInput->SetText(FormatNumber(fDPI));
        fConnections.Connect(fDPIInput->OnEdit,
                             [this](const std::string& v, LWSUI::EditPhase p) { OnDPIChanged(v, p); });
        dpiRow->Add(std::unique_ptr<LWSUI::Control>(fDPIInput));
        auto dpiUnit               = Label("dots/in", LWSUI::Control::FontRole::Small);
        dpiUnit->verticalAlignment = LWSUI::Alignment::Center;
        dpiUnit->SetForeground(LLUtils::Color(134, 146, 165));
        dpiRow->Add(std::move(dpiUnit));
        dpiBox->SetContent(std::move(dpiRow));
        unitsStack->Add(std::move(dpiBox));
        auto unitsPanel     = std::make_unique<LWSUI::Panel>();
        fUnitsPanel         = unitsPanel.get();
        unitsPanel->padding = {6, 3, 6, 3};
        unitsPanel->SetStyle(WithBackground(theme, theme.surface));
        unitsPanel->SetContent(std::move(unitsStack));
        left->Add(std::move(unitsPanel));

        addHeading("Output dimensions");
        auto dimensions     = std::make_unique<LWSUI::Grid>();
        dimensions->columns = {-1, -1};
        dimensions->spacing = 8;
        auto makeDimension  = [&](const char* caption, bool height, LWSUI::TextBox*& edit, LWSUI::Label*& unit)
        {
            auto box = std::make_unique<LWSUI::Panel>();
            box->SetCornerRadius(9);
            box->padding = {8, 4, 8, 4};
            box->SetStyle(WithBackground(theme, theme.surface));
            auto stack     = std::make_unique<LWSUI::StackPanel>();
            stack->spacing = 0;
            auto name      = Label(caption, LWSUI::Control::FontRole::Small);
            name->SetForeground(LLUtils::Color(133, 145, 164));
            name->sizeToContent = true;
            auto heading        = std::make_unique<LWSUI::FlowPanel>();
            heading->gap        = 2;
            heading->AddItem(std::move(name));
            auto reset                    = std::make_unique<LWSUI::Button>("↺");
            fResetButtons[height ? 2 : 1] = reset.get();
            reset->sizeToContent          = true;
            auto resetStyle               = theme;
            resetStyle.rowHeight          = 20;
            resetStyle.padding            = 3;
            reset->SetStyle(resetStyle);
            reset->flat = true;
            reset->SetTooltip(height ? "Reset height" : "Reset width");
            fConnections.Connect(reset->OnClick, [this, height] { ResetDimension(height); });
            heading->AddItem(std::move(reset));
            stack->Add(std::move(heading));
            auto row     = std::make_unique<LWSUI::Grid>();
            row->columns = {-1, 24};
            edit         = new LWSUI::TextBox();
            edit->SetInputFilter(LWSUI::InputFilters::Number(false, true));
            edit->SetFontWeight(600);
            row->Add(std::unique_ptr<LWSUI::Control>(edit));
            unit                    = new LWSUI::Label("in");
            unit->fontRole          = LWSUI::Control::FontRole::Small;
            unit->verticalAlignment = LWSUI::Alignment::End;
            unit->SetForeground(LLUtils::Color(134, 146, 165));
            row->Add(std::unique_ptr<LWSUI::Control>(unit));
            stack->Add(std::move(row));
            auto wheel                   = std::make_unique<LWSUI::Thumbwheel>();
            fThumbwheels[height ? 1 : 0] = wheel.get();
            wheel->SetTooltip("Click right to enlarge, left to shrink. Hold or drag for more. Farther means faster. "
                              "Release to stop. Shift "
                              "gives finer control.");
            fConnections.Connect(wheel->OnEdit, [this, height](double value, LWSUI::EditPhase phase)
                                 { OnThumbwheel(height, value, phase); });
            stack->Add(std::move(wheel));
            box->SetContent(std::move(stack));
            return box;
        };
        dimensions->Add(makeDimension("WIDTH", false, fWidthInput, fWidthUnit));
        fConnections.Connect(fWidthInput->OnEdit,
                             [this](const std::string& v, LWSUI::EditPhase p) { OnDimensionChanged(false, v, p); });
        dimensions->Add(makeDimension("HEIGHT", true, fHeightInput, fHeightUnit));
        fConnections.Connect(fHeightInput->OnEdit,
                             [this](const std::string& v, LWSUI::EditPhase p) { OnDimensionChanged(true, v, p); });
        SetThumbwheelSettings(fInput.thumbwheel);
        left->Add(std::move(dimensions));

        addHeading("Aspect ratio");
        auto ratioPanel = std::make_unique<LWSUI::Panel>();
        ratioPanel->SetCornerRadius(9);
        ratioPanel->padding = {8, 2, 8, 2};
        ratioPanel->SetStyle(WithBackground(theme, LLUtils::Color(241, 243, 247)));
        fAspectRatioRadio = new CompactRadioGroup(
            {{"Uniform", "preserve"}, {"Non-uniform", "don't_preserve"}, {"Letterbox", "preserve_bars"}});
        fAspectRatioRadio->SetValue(fInput.defaults.aspect == ResizeAspectRatio::Preserve        ? "preserve"
                                    : fInput.defaults.aspect == ResizeAspectRatio::DoNotPreserve ? "don't_preserve"
                                                                                                 : "preserve_bars");
        fAspectRatioRadio->orientation = LWSUI::Orientation::Horizontal;
        fConnections.Connect(fAspectRatioRadio->OnChange, [this](const std::string&) { OnAspectRatioChanged(); });
        ratioPanel->SetContent(std::unique_ptr<LWSUI::Control>(fAspectRatioRadio));
        left->Add(std::move(ratioPanel));
        addHeading("Scaler");
        fAutoFilter = new LWSUI::CheckBox("Auto mode");
        fAutoFilter->SetValue(fInput.defaults.autoFilter);
        fAutoFilter->SetTooltip("Mitchell for reduction or enlargement above 2×; Catmull–Rom for enlargement through "
                                "2×. Each axis is chosen separately.");
        fConnections.Connect(fAutoFilter->OnChange, [this](bool) { UpdateCalculatedPixels(); });
        left->Add(std::unique_ptr<LWSUI::Control>(fAutoFilter));
        std::vector<LWSUI::Choice> filters;
        for (auto filter : ResizeFilters)
            filters.push_back({std::string(ResizeFilterName(filter)), std::to_string(static_cast<int>(filter))});
        fFilterCombo = new LWSUI::ComboBox(std::move(filters));
        fFilterCombo->SetValue(std::to_string(static_cast<int>(fManualFilter)));
        fConnections.Connect(fFilterCombo->OnChange,
                             [this](const std::string& value)
                             {
                                 fManualFilter = static_cast<ResizeFilter>(std::stoi(value));
                                 fAutoFilter->SetValue(false);
                                 UpdateCalculatedPixels();
                             });
        left->Add(std::unique_ptr<LWSUI::Control>(fFilterCombo));
        content->Add(std::move(left));

        auto preview = std::make_unique<LWSUI::Panel>();
        preview->SetCornerRadius(9);
        preview->padding = {12, 10, 12, 10};
        preview->SetStyle(WithBackground(theme, LLUtils::Color(247, 249, 252)));
        auto previewStack      = std::make_unique<LWSUI::DockPanel>();
        previewStack->gap      = 6;
        auto previewHeader     = std::make_unique<LWSUI::StackPanel>();
        previewHeader->spacing = 1;
        auto previewTitle      = Label("Preview", LWSUI::Control::FontRole::Heading);
        previewTitle->SetForeground(LLUtils::Color(74, 91, 116));
        previewHeader->Add(std::move(previewTitle));
        auto original = Label("Original · " + FormatPixels(fOriginalWidth, fOriginalHeight),
                              LWSUI::Control::FontRole::Small);
        original->SetForeground(LLUtils::Color(132, 144, 163));
        previewHeader->Add(std::move(original));
        previewStack->AddDocked(std::move(previewHeader), LWSUI::Dock::Top);
        fPreview = new ResizePreview();
        fPreview->SetBitmap(fInput.sourceBitmap, fOriginalWidth / fOriginalHeight);
        previewStack->AddDocked(std::unique_ptr<LWSUI::Control>(fPreview), LWSUI::Dock::Fill);
        // Reserve metadata before assigning the remaining height to the image, including in pixels mode.
        auto previewDetails       = std::make_unique<LWSUI::StackPanel>();
        previewDetails->spacing   = 4;
        fPreviewCaption           = new LWSUI::Label("Uniform scaling");
        fPreviewCaption->fontRole = LWSUI::Control::FontRole::Small;
        fPreviewCaption->SetForeground(LLUtils::Color(114, 128, 152));
        previewDetails->Add(std::unique_ptr<LWSUI::Control>(fPreviewCaption));
        auto printTitle = Label("PRINT SIZE", LWSUI::Control::FontRole::Small);
        printTitle->SetForeground(LLUtils::Color(132, 144, 163));
        previewDetails->Add(std::move(printTitle));
        fPrintSize           = new LWSUI::Label("—");
        fPrintSize->fontRole = LWSUI::Control::FontRole::Heading;
        fPrintSize->SetForeground(LLUtils::Color(42, 57, 82));
        previewDetails->Add(std::unique_ptr<LWSUI::Control>(fPrintSize));
        auto outputTitle = Label("OUTPUT RESOLUTION", LWSUI::Control::FontRole::Small);
        outputTitle->SetForeground(LLUtils::Color(132, 144, 163));
        previewDetails->Add(std::move(outputTitle));
        fPreviewPixels           = new LWSUI::Label("—");
        fPreviewPixels->fontRole = LWSUI::Control::FontRole::Heading;
        fPreviewPixels->SetForeground(LLUtils::Color(42, 57, 82));
        previewDetails->Add(std::unique_ptr<LWSUI::Control>(fPreviewPixels));
        previewStack->AddDocked(std::move(previewDetails), LWSUI::Dock::Bottom);
        preview->SetContent(std::move(previewStack));
        content->Add(std::move(preview));
        root->AddDocked(std::move(content), LWSUI::Dock::Fill);

        auto footer           = std::make_unique<LWSUI::FlowPanel>();
        footer->gap           = 8;
        footer->alignEnd      = true;
        auto cancel           = std::make_unique<LWSUI::Button>("Cancel");
        cancel->sizeToContent = true;
        fConnections.Connect(cancel->OnClick, [this] { Dismiss(); });
        footer->AddItem(std::move(cancel));
        fApplyButton                    = new LWSUI::Button("Resize image");
        fApplyButton->sizeToContent     = true;
        auto primaryButtonStyle         = theme;
        primaryButtonStyle.line         = LLUtils::Color(40, 101, 213);
        primaryButtonStyle.surface      = LLUtils::Color(215, 223, 233);
        primaryButtonStyle.muted        = LLUtils::Color(102, 117, 139);
        primaryButtonStyle.hoverSurface = LLUtils::Color(34, 88, 190);
        primaryButtonStyle.selectedRow  = LLUtils::Color(29, 75, 165);
        fApplyButton->SetStyle(std::move(primaryButtonStyle));
        fApplyButton->SetForeground(LLUtils::Color(255, 255, 255));
        fApplyButton->fontRole = LWSUI::Control::FontRole::Heading;
        fConnections.Connect(fApplyButton->OnClick, [this] { Apply(); });
        footer->AddItem(std::unique_ptr<LWSUI::Control>(fApplyButton));
        auto footerRow     = std::make_unique<LWSUI::Grid>();
        footerRow->columns = {64, -1, 205};
        footerRow->spacing = 8;
        auto resetAll      = std::make_unique<LWSUI::Button>("Reset");
        fResetButtons[0]   = resetAll.get();
        resetAll->SetTooltip("Restore initialization settings");
        fConnections.Connect(resetAll->OnClick, [this] { ResetAll(); });
        footerRow->Add(std::move(resetAll));
        fStatusLabel           = new LWSUI::Label();
        fStatusLabel->fontRole = LWSUI::Control::FontRole::Small;
        fStatusLabel->wrap     = true;
        footerRow->Add(std::unique_ptr<LWSUI::Control>(fStatusLabel));
        footerRow->Add(std::move(footer));
        auto footerPanel     = std::make_unique<FooterPanel>();
        footerPanel->padding = {0, 6, 0, 4};
        footerPanel->SetContent(std::move(footerRow));
        root->AddDocked(std::move(footerPanel), LWSUI::Dock::Bottom, 36);
        auto draggable               = std::make_unique<DraggableSurface>(std::move(root));
        draggable->padding           = {14, 8, 14, 8};
        draggable->paintBackground   = false;
        draggable->hitTestBackground = true;
        fUIHost->SetRoot(std::move(draggable));
        UpdateFieldsFromBasis();
        UpdateVisibility();
        UpdateCalculatedPixels();
    }

    void ResizeFormController::Hide()
    {
        if (fDialogWindow)
            std::ignore = fDialogWindow->SetVisible(false);
    }
    void ResizeFormController::Dismiss()
    {
        if (fUIHost)
        {
            fUIHost->ClosePopup(LWSUI::EditPhase::Cancel);
            if (fUIHost->Root())
                fUIHost->Root()->Finish(LWSUI::EditPhase::Cancel);
        }
        if (fInput.onDismiss)
            fInput.onDismiss();
        Hide();
    }
    void ResizeFormController::SetThumbwheelSettings(const ResizeThumbwheelSettings& settings)
    {
        for (auto* wheel : fThumbwheels)
            if (wheel)
            {
                wheel->SetSpeedRange(settings.startSpeed, settings.maximumSpeed);
                wheel->SetAcceleration(settings.acceleration);
                wheel->SetDeadZone(settings.deadZonePercent / 100);
                wheel->SetDragSensitivity(settings.dragSensitivity);
                wheel->SetFineModeMultiplier(settings.fineModeMultiplier);
            }
        fInput.thumbwheel = settings;
    }
    void ResizeFormController::SetError(std::string message)
    {
        fStatusLabel->SetForeground(LLUtils::Color(180, 35, 24));
        fStatusLabel->SetText(std::move(message));
    }
    void ResizeFormController::SetBusy(bool busy)
    {
        fBusy = busy;
        for (auto* control :
             std::array<LWSUI::Control*, 8>{fWidthInput, fHeightInput, fDPIInput, fUnitsRadio, fSizeModeRadio,
                                            fAspectRatioRadio, fStandardResolutions, fAutoFilter})
            control->SetEnabled(!busy);
        for (auto* control : fResetButtons)
            control->SetEnabled(!busy);
        for (auto* control : fThumbwheels)
            control->SetEnabled(!busy);
        fStatusLabel->SetForeground(fUIHost->Style().muted);
        fStatusLabel->SetText(busy ? "Resizing…" : "");
        UpdateCalculatedPixels();
    }
    void ResizeFormController::ResetAll()
    {
        if (fBusy)
            return;
        fUIHost->ClosePopup(LWSUI::EditPhase::Cancel);
        fUIHost->Root()->Finish(LWSUI::EditPhase::Cancel);
        fWheelBasis.reset();
        fDPIEditBasis.reset();
        fSizeBasis        = fInitialBasis;
        fDPI              = fInitialDPI;
        fCurrentSizeMode  = fInput.defaults.sizeMode;
        fCurrentUnits     = fInput.defaults.units;
        fManualFilter     = fInput.defaults.filter;
        fLastEditedHeight = false;
        fSizeModeRadio->SetValue(fCurrentSizeMode == ResizeSizeMode::Relative ? "relative" : "absolute");
        fUnitsRadio->SetValue(fCurrentUnits == ResizeUnits::Pixels   ? "pixels"
                              : fCurrentUnits == ResizeUnits::Inches ? "inches"
                                                                     : "centimeters");
        fAspectRatioRadio->SetValue(fInput.defaults.aspect == ResizeAspectRatio::Preserve        ? "preserve"
                                    : fInput.defaults.aspect == ResizeAspectRatio::DoNotPreserve ? "don't_preserve"
                                                                                                 : "preserve_bars");
        fAutoFilter->SetValue(fInput.defaults.autoFilter);
        fFilterCombo->SetValue(std::to_string(static_cast<int>(fManualFilter)));
        fStandardResolutions->SetValue("current");
        fDPIInput->SetText(FormatNumber(fDPI));
        fStatusLabel->SetText("");
        fStatusLabel->SetForeground(fUIHost->Style().muted);
        UpdateFieldsFromBasis();
        UpdateVisibility();
        UpdateCalculatedPixels();
    }
    void ResizeFormController::AcceptDimensionBasis(const SizeBasis& basis, bool height)
    {
        fSizeBasis        = basis;
        fLastEditedHeight = height;
        fStandardResolutions->SetValue("custom");
        UpdateFieldsFromBasis();
        UpdateCalculatedPixels();
    }
    void ResizeFormController::ResetDimension(bool height)
    {
        if (fBusy)
            return;
        fUIHost->Root()->Finish(LWSUI::EditPhase::Cancel);
        const auto dpi = CurrentDPI();
        if (!dpi)
            return;
        const auto original = PixelSize{fOriginalWidth, fOriginalHeight};
        if (!fInitialPixels)
            return;
        const SizeBasis initial{ResizeSizeMode::Absolute, ResizeUnits::Pixels, fInitialPixels->width,
                                fInitialPixels->height};
        const auto dimensions = initial.Dimensions(fCurrentSizeMode, fCurrentUnits, original, *dpi);
        if (!dimensions)
            return;
        if (auto basis = fSizeBasis.WithDimension(height ? dimensions->height : dimensions->width, height,
                                                  fCurrentSizeMode, fCurrentUnits,
                                                  fAspectRatioRadio->Value() == "preserve", original, *dpi))
            AcceptDimensionBasis(*basis, height);
    }
    void ResizeFormController::OnThumbwheel(bool height, double displacement, LWSUI::EditPhase phase)
    {
        if (fBusy)
            return;
        if (phase == LWSUI::EditPhase::Cancel)
        {
            if (fWheelBasis)
                fSizeBasis = *fWheelBasis;
            fWheelBasis.reset();
            UpdateFieldsFromBasis();
            UpdateCalculatedPixels();
            return;
        }
        if (!fWheelBasis)
        {
            if (!fWidthInput->Finish(LWSUI::EditPhase::Commit) || !fHeightInput->Finish(LWSUI::EditPhase::Commit) ||
                !fDPIInput->Finish(LWSUI::EditPhase::Commit))
                return;
            fWheelHeight = height;
            fWheelBasis  = fSizeBasis;
            fWheelOffset = 0;
        }
        const auto original = PixelSize{fOriginalWidth, fOriginalHeight};
        const auto dpi      = CurrentDPI();
        if (!dpi)
        {
            fWheelBasis.reset();
            return;
        }
        const auto dimensions = fWheelBasis->Dimensions(fCurrentSizeMode, fCurrentUnits, original, *dpi);
        // A zero displacement restores the exact starting basis, including a
        // deliberately non-proportional initialization shown in Uniform mode.
        if (displacement == 0 && fWheelOffset == 0)
        {
            fSizeBasis = *fWheelBasis;
            UpdateFieldsFromBasis();
            UpdateCalculatedPixels();
        }
        else if (dimensions)
        {
            const bool preserve   = fAspectRatioRadio->Value() == "preserve";
            const double step     = fCurrentSizeMode == ResizeSizeMode::Relative ? .1
                                    : fCurrentUnits == ResizeUnits::Pixels       ? 1
                                                                                 : .01;
            const double start    = height ? dimensions->height : dimensions->width;
            const auto unitBasis  = fWheelBasis->WithDimension(1, height, fCurrentSizeMode, fCurrentUnits, preserve,
                                                               original, *dpi);
            const auto unitPixels = unitBasis ? unitBasis->Pixels(original, *dpi) : std::nullopt;
            if (!unitPixels)
                return;
            double coefficient = height ? unitPixels->height : unitPixels->width;
            double minimum = 1 / coefficient, maximum = INT32_MAX / coefficient;
            if (preserve)
            {
                coefficient = height ? unitPixels->width : unitPixels->height;
                minimum     = std::max(minimum, 1 / coefficient);
                maximum     = std::min(maximum, INT32_MAX / coefficient);
            }
            const double requested = start + (displacement - fWheelOffset) * step;
            double value           = std::clamp(requested, minimum, maximum);
            // Drop excess movement at bounds while retaining the fractional accumulator
            // during ordinary snapping, so fine movement and reversals remain responsive.
            if (value != requested)
                fWheelOffset = displacement - (value - start) / step;
            if (fCurrentSizeMode == ResizeSizeMode::Absolute && fCurrentUnits == ResizeUnits::Pixels)
                value = std::clamp(std::round(value), std::ceil(minimum), std::floor(maximum));
            const auto basis  = fWheelBasis->WithDimension(value, height, fCurrentSizeMode, fCurrentUnits, preserve,
                                                           original, *dpi);
            const auto pixels = basis ? basis->Pixels(original, *dpi) : std::nullopt;
            if (pixels && pixels->width >= 1 && pixels->height >= 1 && pixels->width <= INT32_MAX &&
                pixels->height <= INT32_MAX)
            {
                fSizeBasis = *basis;
                UpdateFieldsFromBasis();
                UpdateCalculatedPixels();
            }
        }
        if (phase == LWSUI::EditPhase::Commit)
        {
            const auto start   = fWheelBasis->Pixels(original, *dpi);
            const auto current = fSizeBasis.Pixels(original, *dpi);
            if (start && current && (start->width != current->width || start->height != current->height))
                AcceptDimensionBasis(fSizeBasis, height);
            fWheelBasis.reset();
        }
    }
    void ResizeFormController::Apply()
    {
        if (fBusy || !fInput.onResize || !fWidthInput->Finish(LWSUI::EditPhase::Commit) ||
            !fHeightInput->Finish(LWSUI::EditPhase::Commit) || !fDPIInput->Finish(LWSUI::EditPhase::Commit))
            return;
        const auto request = CurrentRequest();
        if (!request)
        {
            SetError("Enter valid dimensions and DPI within the supported range");
            return;
        }
        const LLUtils::PointI32 original{static_cast<int32_t>(fOriginalWidth), static_cast<int32_t>(fOriginalHeight)};
        const auto layout = ResolveImageResize(original, *request);
        if (!layout)
            SetError(layout.error());
        else if (layout->canvas != original)
            fInput.onResize(*request);
    }
    std::optional<ImageResizeRequest> ResizeFormController::CurrentRequest() const
    {
        const auto pixels = CurrentPixels();
        const auto dpi    = CurrentDPI();
        if (!pixels || !dpi || pixels->width > INT32_MAX || pixels->height > INT32_MAX)
            return std::nullopt;
        return ImageResizeRequest{.size       = {std::max(1, static_cast<int32_t>(std::round(pixels->width))),
                                                 std::max(1, static_cast<int32_t>(std::round(pixels->height)))},
                                  .aspect     = fAspectRatioRadio->Value() == "preserve" ? ResizeAspectRatio::Preserve
                                                : fAspectRatioRadio->Value() == "preserve_bars"
                                                    ? ResizeAspectRatio::BlackBars
                                                    : ResizeAspectRatio::DoNotPreserve,
                                  .dpi        = *dpi,
                                  .autoFilter = fAutoFilter->Value(),
                                  .filter     = fManualFilter,
                                  .preserveHeight = fWheelBasis       ? fWheelHeight
                                                    : fDimensionDraft ? fDraftHeight
                                                                      : fLastEditedHeight};
    }
    std::optional<ImageResizeLayout> ResizeFormController::CurrentLayout() const
    {
        if (const auto request = CurrentRequest())
            if (const auto layout = ResolveImageResize(
                    {static_cast<int32_t>(fOriginalWidth), static_cast<int32_t>(fOriginalHeight)}, *request))
                return *layout;
        return std::nullopt;
    }
    void ResizeFormController::UpdateFilter(const ImageResizeLayout* layout)
    {
        fFilterCombo->SetEnabled(!fBusy);
        fFilterCombo->SetValue(std::to_string(static_cast<int>(fManualFilter)));
        if (fAutoFilter->Value() && layout)
        {
            auto caption = std::string(ResizeFilterName(layout->horizontal));
            if (layout->horizontal != layout->vertical)
                caption += " (H) / " + std::string(ResizeFilterName(layout->vertical)) + " (V)";
            fFilterCombo->SetText(std::move(caption));
        }
    }

    void ResizeFormController::UpdateVisibility()
    {
        const bool showUnits = fCurrentSizeMode == ResizeSizeMode::Absolute;
        const bool showDpi   = showUnits && fCurrentUnits != ResizeUnits::Pixels;
        fUnitsRadio->SetVisible(showUnits);
        fUnitsPanel->SetVisible(showUnits);
        fUnitsTitle->SetVisible(showUnits);
        fDPIBox->SetVisible(showDpi);
        if (fUIHost)
        {
            fUIHost->Invalidate(true);
            ResizeDialog();
        }
    }
    void ResizeFormController::ResizeDialog()
    {
        if (!fDialogWindow)
            return;
        const auto measured = fUIHost->Root()->Measure({float(DialogWidth), 10000});
        fDialogSize         = {DialogWidth, static_cast<int32_t>(std::ceil(measured.height))};
        std::ignore         = fDialogWindow->RequestPlacement({.clientSize = fDialogSize});
    }
    void ResizeFormController::OnSizeModeChanged(const std::string& value)
    {
        if (value == "relative")
            SetSizeMode(ResizeSizeMode::Relative);
        else if (value == "absolute")
            SetSizeMode(ResizeSizeMode::Absolute);
    }
    void ResizeFormController::SetSizeMode(ResizeSizeMode mode)
    {
        fCurrentSizeMode = mode;
        UpdateVisibility();
        if (mode == ResizeSizeMode::Relative && fAspectRatioRadio->Value() == "preserve")
            OnAspectRatioChanged();
        UpdateFieldsFromBasis();
        UpdateCalculatedPixels();
    }
    void ResizeFormController::OnUnitsChanged(const std::string& value)
    {
        if (value == "pixels")
            fCurrentUnits = ResizeUnits::Pixels;
        else if (value == "inches")
            fCurrentUnits = ResizeUnits::Inches;
        else if (value == "centimeters")
            fCurrentUnits = ResizeUnits::Centimeters;
        if (fCurrentSizeMode == ResizeSizeMode::Absolute)
        {
            UpdateFieldsFromBasis();
            UpdateVisibility();
        }
        UpdateCalculatedPixels();
    }
    void ResizeFormController::OnDPIChanged(const std::string&, LWSUI::EditPhase phase)
    {
        if (phase == LWSUI::EditPhase::Cancel)
        {
            if (fDPIEditBasis)
            {
                fSizeBasis = *fDPIEditBasis;
                fDPI       = fDPIEditStart;
                fDPIEditBasis.reset();
            }
            UpdateFieldsFromBasis();
            UpdateCalculatedPixels();
            return;
        }
        if (phase != LWSUI::EditPhase::Commit && phase != LWSUI::EditPhase::Preview)
            return;
        if (!fDPIEditBasis)
        {
            fDPIEditBasis = fSizeBasis;
            fDPIEditStart = fDPI;
        }
        if (const auto dpi = CurrentDPI())
        {
            // Freeze the exact physical size at the start of the edit, before the new DPI is applied.
            // Every preview derives from that snapshot so partial typing and cancellation cannot introduce drift.
            if (*dpi == fDPIEditStart)
                fSizeBasis = *fDPIEditBasis;
            else if (auto basis = fDPIEditBasis->ForDPIEdit(fCurrentSizeMode, fCurrentUnits,
                                                            {fOriginalWidth, fOriginalHeight}, fDPIEditStart))
                fSizeBasis = *basis;
            fDPI = *dpi;
            if (phase == LWSUI::EditPhase::Commit && *dpi != fDPIEditStart &&
                fCurrentSizeMode == ResizeSizeMode::Absolute && fCurrentUnits != ResizeUnits::Pixels)
                fStandardResolutions->SetValue("custom");
            UpdateFieldsFromBasis();
        }
        if (phase == LWSUI::EditPhase::Commit)
            fDPIEditBasis.reset();
        UpdateCalculatedPixels();
    }
    void ResizeFormController::OnStandardResolutionChanged(const std::string& value)
    {
        if (value == "custom")
            return;
        double width = fOriginalWidth, height = fOriginalHeight;
        if (value != "current")
        {
            const auto split = value.find('x');
            if (split == std::string::npos)
                return;
            try
            {
                width  = std::stod(value.substr(0, split));
                height = std::stod(value.substr(split + 1));
            }
            catch (...)
            {
                return;
            }
        }
        fSizeModeRadio->SetValue("absolute");
        fUnitsRadio->SetValue("pixels");
        fCurrentSizeMode = ResizeSizeMode::Absolute;
        fCurrentUnits    = ResizeUnits::Pixels;
        fSizeBasis       = {ResizeSizeMode::Absolute, ResizeUnits::Pixels, width, height};
        if (fAspectRatioRadio->Value() == "preserve" &&
            PresetResizeAspect(ResizeAspectRatio::Preserve,
                               {static_cast<int32_t>(fOriginalWidth), static_cast<int32_t>(fOriginalHeight)},
                               {static_cast<int32_t>(width), static_cast<int32_t>(height)}) ==
                ResizeAspectRatio::BlackBars)
            fAspectRatioRadio->SetValue("preserve_bars");
        UpdateFieldsFromBasis();
        UpdateVisibility();
        UpdateCalculatedPixels();
    }
    void ResizeFormController::OnAspectRatioChanged()
    {
        if (fAspectRatioRadio->Value() == "preserve")
        {
            if (auto basis = fSizeBasis.Preserved(fLastEditedHeight, {fOriginalWidth, fOriginalHeight},
                                                  CurrentDPI().value_or(fDPI)))
            {
                fSizeBasis = *basis;
                fStandardResolutions->SetValue("custom");
                UpdateFieldsFromBasis();
            }
        }
        UpdateCalculatedPixels();
    }
    std::optional<double> ResizeFormController::CurrentDPI() const
    {
        try
        {
            const double dpi = fDPIInput ? std::stod(fDPIInput->Text()) : fDPI;
            if (std::isfinite(dpi) && dpi > 0)
                return dpi;
        }
        catch (...)
        {
        }
        return std::nullopt;
    }
    std::optional<ResizeFormController::PixelSize> ResizeFormController::PixelsFromBasis() const
    {
        return fSizeBasis.Pixels({fOriginalWidth, fOriginalHeight}, CurrentDPI().value_or(fDPI));
    }
    std::optional<ResizeFormController::PixelSize> ResizeFormController::CurrentPixels() const
    {
        return fDimensionDraft ? fDraftPixels : PixelsFromBasis();
    }
    void ResizeFormController::UpdateFieldsFromBasis()
    {
        if (fUpdatingFields)
            return;
        const auto dimensions = fSizeBasis.Dimensions(fCurrentSizeMode, fCurrentUnits,
                                                      {fOriginalWidth, fOriginalHeight}, CurrentDPI().value_or(fDPI));
        if (!dimensions)
            return;
        fDimensionDraft = false;
        fDraftPixels.reset();
        fUpdatingFields = true;
        // Display the same whole pixels sent to the resizer, while retaining the exact
        // proportional basis for future edits and physical-unit conversions.
        const bool pixelsMode = fCurrentSizeMode == ResizeSizeMode::Absolute && fCurrentUnits == ResizeUnits::Pixels;
        auto display          = *dimensions;
        if (pixelsMode)
            if (const auto layout = CurrentLayout())
                display = {double(layout->canvas.x), double(layout->canvas.y)};
        fDisplayedWidth  = FormatResizeDimension(display.width);
        fDisplayedHeight = FormatResizeDimension(display.height);
        fWidthInput->SetText(fDisplayedWidth);
        fHeightInput->SetText(fDisplayedHeight);
        fUpdatingFields = false;
    }
    void ResizeFormController::OnDimensionChanged(bool height, const std::string& text, LWSUI::EditPhase phase)
    {
        if (fUpdatingFields)
            return;
        if (phase == LWSUI::EditPhase::Cancel)
        {
            fDimensionDraft = false;
            fDraftPixels.reset();
            UpdateFieldsFromBasis();
            UpdateCalculatedPixels();
            return;
        }
        if (phase != LWSUI::EditPhase::Commit && phase != LWSUI::EditPhase::Preview)
            return;

        const auto& displayed = height ? fDisplayedHeight : fDisplayedWidth;
        const auto original   = ResizeFormDimensions{fOriginalWidth, fOriginalHeight};
        const auto dpi        = CurrentDPI().value_or(fDPI);
        const auto edited     = fSizeBasis.Edited(text, displayed, height, fCurrentSizeMode, fCurrentUnits,
                                                  fAspectRatioRadio->Value() == "preserve", original, dpi);
        fDraftHeight          = text != displayed ? height : fLastEditedHeight;
        fDimensionDraft       = true;
        fDraftPixels          = edited ? edited->Pixels(original, dpi) : std::nullopt;
        if (phase == LWSUI::EditPhase::Commit && edited)
        {
            if (text != displayed)
            {
                fLastEditedHeight = height;
                fStandardResolutions->SetValue("custom");
            }
            fSizeBasis      = *edited;
            fDimensionDraft = false;
            fDraftPixels.reset();
            UpdateFieldsFromBasis();
        }
        else if (edited && fAspectRatioRadio->Value() == "preserve")
        {
            auto dimensions = edited->Dimensions(fCurrentSizeMode, fCurrentUnits, original, dpi);
            if (fCurrentSizeMode == ResizeSizeMode::Absolute && fCurrentUnits == ResizeUnits::Pixels)
                if (const auto layout = CurrentLayout())
                    dimensions = PixelSize{double(layout->canvas.x), double(layout->canvas.y)};
            fUpdatingFields = true;
            (height ? fWidthInput : fHeightInput)
                ->SetText(FormatResizeDimension(height ? dimensions->width : dimensions->height));
            fUpdatingFields = false;
        }
        UpdateCalculatedPixels();
    }
    void ResizeFormController::UpdateCalculatedPixels()
    {
        fPreviewCaption->SetText(fAspectRatioRadio->Value() == "preserve"        ? "Uniform scaling"
                                 : fAspectRatioRadio->Value() == "preserve_bars" ? "Letterbox · image fitted"
                                                                                 : "Non-uniform scaling");
        const LLUtils::PointI32 original{static_cast<int32_t>(fOriginalWidth), static_cast<int32_t>(fOriginalHeight)};
        const auto layout      = CurrentLayout();
        const bool valid       = layout.has_value();
        const auto dpi         = CurrentDPI();
        const double outWidth  = valid ? layout->canvas.x : fOriginalWidth;
        const double outHeight = valid ? layout->canvas.y : fOriginalHeight;
        const auto pixels      = FormatPixels(outWidth, outHeight);
        fPreviewPixels->SetText(valid ? pixels : "—");
        if (valid)
            fPreview->SetAspect(outWidth / outHeight);
        fPreview->SetBlackBars(fAspectRatioRadio->Value() == "preserve_bars");
        const auto suffix = fCurrentSizeMode == ResizeSizeMode::Relative ? "%"
                            : fCurrentUnits == ResizeUnits::Pixels       ? "px"
                            : fCurrentUnits == ResizeUnits::Inches       ? "in"
                                                                         : "cm";
        fWidthUnit->SetText(suffix);
        fHeightUnit->SetText(suffix);
        if (valid)
        {
            const bool useCentimeters = fCurrentSizeMode == ResizeSizeMode::Absolute &&
                                        fCurrentUnits == ResizeUnits::Centimeters;
            const double printDpi    = dpi.value_or(fDPI);
            const double printWidth  = outWidth / printDpi * (useCentimeters ? 2.54 : 1.0);
            const double printHeight = outHeight / printDpi * (useCentimeters ? 2.54 : 1.0);
            const auto unit          = useCentimeters ? "cm" : "in";
            fPrintSize->SetText(FormatNumber(printWidth) + " × " + FormatNumber(printHeight) + " " + unit + " × " +
                                FormatNumber(printDpi) + " DPI");
        }
        else
            fPrintSize->SetText(fCurrentSizeMode == ResizeSizeMode::Absolute && fCurrentUnits != ResizeUnits::Pixels &&
                                        !dpi
                                    ? "Enter valid DPI"
                                    : "Enter valid dimensions");
        // Compare final integer canvas dimensions, including Preserve's correction, rather than rounded labels.
        const bool changesSize = valid && layout->canvas != original;
        fApplyButton->SetEnabled(!fBusy && changesSize && static_cast<bool>(fInput.onResize));
        fApplyButton->SetTooltip(valid && !changesSize ? "Output dimensions are identical to the original image" : "");
        UpdateFilter(valid ? &*layout : nullptr);
    }
}  // namespace OIV
