#include "ImageBitmap.h"
#include "MainWindow.h"

#include <ImageLoader.h>
#include <LWS/Platform.hpp>
#include <LWSUI/UIHost.hpp>
#include <LWSUI/Primitives.hpp>
#include <LWSUI/Containers.hpp>
#include <LLUtils/StringUtility.h>
#include <ImageUtil/ImageUtil.h>
#include <LLUtils/PlatformUtility.h>

#include <cmath>
#include <cctype>
#include <exception>
#include <filesystem>
#include <algorithm>
#include <utility>

namespace OIV
{
    namespace
    {
        constexpr float SidebarMaximumWidth = 260;
        constexpr float SidebarDividerWidth = 5;

        // Reserve UI layout space for the native renderer without intercepting image input.
        class CanvasSlot final : public LWSUI::Control
        {
          public:

            explicit CanvasSlot(LWS::Window& window) : fWindow(window) { hitTestVisible = false; }

          protected:

            void OnArrange() override
            {
                if (!Parent()->Visible())
                    return;
                const auto b = Bounds();
                std::ignore  = fWindow.RequestPlacement(
                    {.position   = LWS::Point{int32_t(std::lround(b.x)), int32_t(std::lround(b.y))},
                      .clientSize = LWS::LogicalSize{std::max(1, int32_t(std::lround(b.width))),
                                                    std::max(1, int32_t(std::lround(b.height)))}});
            }

          private:

            LWS::Window& fWindow;
        };
    }  // namespace

    LWS::Result MainWindow::Create(const LWS::WindowConfig& config)
    {
        SetApplicationIcon();
        const bool showAfterCreate = config.visible;
        auto hiddenConfig          = config;
        hiddenConfig.visible       = false;
        const LWS::Result result   = fWindow.Create(hiddenConfig);
        if (result != LWS::Result::Success)
            return result;

        const LWS::Result chromeResult = OnCreate();
        if (chromeResult != LWS::Result::Success)
            return chromeResult;
        return showAfterCreate ? SetVisible(true) : LWS::Result::Success;
    }

    LWS::Result MainWindow::SetVisible(bool visible)
    {
        const bool firstShow = visible && !fWindow.IsVisible();
        const auto result    = fWindow.SetVisible(visible);
        if (result != LWS::Result::Success || !firstShow)
            return result;

        // Render the populated menu before the first native paint is dispatched.
        if (fChromeHost)
            fChromeHost->Update();
        return LWS::Result::Success;
    }

    void MainWindow::FitToParent()
    {
        if (auto* parent = fWindow.GetParent(); parent != nullptr && parent->IsCreated())
        {
            auto size   = parent->GetClientAreaMetrics().logical;
            size.x      = std::max(size.x, 1);
            size.y      = std::max(size.y, 1);
            std::ignore = fWindow.RequestPlacement({.position = LWS::Point{0, 0}, .clientSize = size});
        }
    }

    void MainWindow::SetCursorType(CursorType type)
    {
        if (type == fCurrentCursorType || type < CursorType::SystemDefault || type >= CursorType::Count)
            return;

        if (type != CursorType::SystemDefault && type <= CursorType::SizeAll && !fCursorsInitialized)
            InitializeCursors();

        fCurrentCursorType = type;
        const auto cursor  = type == CursorType::SystemDefault ? LWS::Cursor::FromShape(LWS::CursorShape::Arrow)
                                                               : fCursors[static_cast<size_t>(type) - 1];
        std::ignore        = fWindow.SetMouseCursor(cursor);
        std::ignore        = fCanvasWindow.SetMouseCursor(cursor);
    }

    void MainWindow::InitializeCursors()
    {
        // Load once on first use so ordinary startup does not decode auto-scroll artwork.
        // These bundled cursors all have centered hotspots. Keep the system shape if loading fails.
        constexpr std::array names{"ArrowC.cur", "ArrowE.cur",  "ArrowNE.cur", "ArrowN.cur", "ArrowNW.cur",
                                   "ArrowW.cur", "ArrowSW.cur", "ArrowS.cur",  "ArrowSE.cur"};
        const auto folder = fResources / "Cursors";
        IMCodec::ImageLoader loader;
        for (size_t index = 0; index < names.size(); ++index)
        {
            try
            {
                IMCodec::ImageSharedPtr image;
                if (loader.Decode((folder / names[index]).native(), IMCodec::ImageLoadFlags::None, {},
                                  IMCodec::PluginTraverseMode::AnyFileType, image) == IMCodec::ImageResult::Success &&
                    image)
                {
                    image = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                             false);
                    if (image != nullptr)
                    {
                        const auto bitmap = MakeBgraBitmapView(*image);
                        // CUR files embed a hotspot, but we currently assume the cursor's center.
                        // This matches the bundled assets. A cross-platform loader should preserve the
                        // embedded hotspot alongside the decoded bitmap and pass it to LWS explicitly.
                        if (auto cursor = LWS::Cursor::FromBitmap(bitmap, {static_cast<int32_t>(bitmap.width / 2),
                                                                           static_cast<int32_t>(bitmap.height / 2)}))
                        {
                            fCursors[index] = *cursor;
                            if (index == 0)
                                fCursors[static_cast<size_t>(CursorType::SizeAll) - 1] = *cursor;
                        }
                    }
                }
            }
            catch (const std::exception&)
            {
                // Cursor artwork is optional. Leave this entry on its system fallback.
            }
        }
        fCursorsInitialized = true;
    }

    LWS::Result MainWindow::OnCreate()
    {
        // The canvas owns image gestures. Passing its mouse hits through to the chrome host
        // lets the UI's context-menu handling consume right-button presses before panning starts.
        const LWS::WindowConfig canvasConfig{
            .parent   = &fWindow,
            .position = LWS::Point{0, 0},
        };
        const LWS::Result result = fCanvasWindow.Create(canvasConfig);
        if (result != LWS::Result::Success)
        {
            std::ignore = fWindow.Destroy();
            return result;
        }
        // The chrome and sidebar share one UI surface. The native canvas remains separate
        // so repainting controls cannot overwrite the renderer, including in embedded viewers.
        LWSUI::Theme theme;
        theme.font.size      = 13;
        theme.menuBarHeight  = 32;
        theme.menuBarPadding = 9;
        theme.menuRowHeight  = 24;
        theme.menuPadding    = 2;
        theme.menuMaxRows    = 32;
        fChromeHost          = std::make_unique<LWSUI::UIHost>(fWindow, theme);
        if (!fWindow.GetParent())
        {
            auto bar = std::make_unique<LWSUI::MenuBar>();
            bar->SetIcon(fApplicationIcon);
            bar->SetWindowControls({
                .minimize            = true,
                .maximize            = true,
                .draggable           = true,
                .requestClose        = requestClose,
                .doubleClickMaximize = true,
            });
            auto title               = std::make_unique<LWSUI::Label>();
            title->hitTestVisible    = false;
            title->verticalAlignment = LWSUI::Alignment::Center;
            title->trimming          = LWSUI::TextTrimming::Ellipsis;
            fTitleLabel              = title.get();
            auto caption             = std::make_unique<LWSUI::Panel>(std::move(title));
            caption->paintBackground = false;
            caption->border.left     = 1;
            caption->padding.left    = 10;
            auto inset               = std::make_unique<LWSUI::Panel>(std::move(caption));
            inset->paintBackground   = false;
            inset->hitTestVisible    = false;
            inset->padding           = {6, 0, 0, 0};
            bar->SetContent(std::move(inset));
            fChromeHost->SetMainMenu(std::move(bar));
        }
        auto images   = std::make_unique<ImageControl>();
        fImageControl = images.get();
        images->SetVisible(fShowImageControl);
        auto split = std::make_unique<LWSUI::SplitPanel>(std::make_unique<CanvasSlot>(fCanvasWindow), std::move(images),
                                                         LWSUI::Orientation::Horizontal);
        fSidebarSplit = split.get();
        split->SetDefaultTrailingSize(160);
        split->SetMinimumSizes(100, 90);
        split->SetDividerSize(SidebarDividerWidth);
        auto sidebar               = std::make_unique<LWSUI::Panel>(std::move(split));
        fSidebarLayout             = sidebar.get();
        sidebar->paintBackground   = false;
        sidebar->hitTestBackground = false;
        fChromeHost->SetRoot(std::move(sidebar));
        fChromeHost->SetRedrawOnResize(true);
        SetTitle(fWindow.GetTitle());
        ApplyInterfacePreferences(fInterface);
        UpdateLayout();
        return LWS::Result::Success;
    }

    void MainWindow::SetTitle(const LWS::string_type& title)
    {
        std::ignore = fWindow.SetTitle(title);
        if (fTitleLabel)
            fTitleLabel->SetText(LLUtils::StringUtility::ConvertString<std::string>(title));
    }

    void MainWindow::SetMenuProvider(std::function<std::vector<LWSUI::MenuBarItem>()> provider)
    {
        fMenuProvider = std::move(provider);
        RefreshMenu();
    }

    void MainWindow::RefreshMenu()
    {
        if (fChromeHost && fChromeHost->MainMenu())
            fChromeHost->MainMenu()->SetItems(fInterface.caption && fInterface.menuBar && fMenuProvider
                                                  ? fMenuProvider()
                                                  : std::vector<LWSUI::MenuBarItem>{});
    }

    bool MainWindow::MenuOwnsKeyboard() const
    {
        return fChromeHost && fChromeHost->HasOpenMenu();
    }

    bool MainWindow::DeferShortcutToMenu(LWS::KeyCode key, bool control, bool alt, bool shift) const
    {
        // Modified function keys remain application commands while the menu is active.
        if (!MenuOwnsKeyboard() || control || (shift && (key == LWS::KeyCode::F10 || key == LWS::KeyCode::F11)))
            return false;
        if (!alt || key == LWS::KeyCode::Alt || key == LWS::KeyCode::F10)
            return true;
        const auto matches = [key](std::string_view label)
        {
            const auto mark = label.find('&');
            if (mark == std::string_view::npos || mark + 1 == label.size())
                return false;
            const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(label[mark + 1])));
            return c >= 'A' && c <= 'Z' &&
                   key == static_cast<LWS::KeyCode>(std::to_underlying(LWS::KeyCode::A) + c - 'A');
        };
        const auto inItems = [&](auto&& self, const std::vector<LWSUI::MenuItem>& items) -> bool
        {
            return std::ranges::any_of(items, [&](const auto& item)
                                       { return matches(item.label) || self(self, item.submenu); });
        };
        return std::ranges::any_of(fChromeHost->MainMenu()->Items(), [&](const auto& item)
                                   { return matches(item.label) || inItems(inItems, item.items); });
    }

    void MainWindow::PrepareMenu(const LWS::AnyEvent& event)
    {
        if (!fChromeHost || !fChromeHost->MainMenu() || MenuOwnsKeyboard())
            return;
        bool refresh = false;
        if (const auto* key = std::get_if<LWS::EventKeyDown>(&event))
        {
            const auto& platform = fWindow.GetPlatformContext();
            const bool modified  = platform.IsKeyPressed(LWS::KeyCode::Control).value_or(false) ||
                                  platform.IsKeyPressed(LWS::KeyCode::Shift).value_or(false);
            refresh = !modified && !key->repeat &&
                      (key->key == LWS::KeyCode::Alt || key->key == LWS::KeyCode::LAlt ||
                       key->key == LWS::KeyCode::RAlt || key->key == LWS::KeyCode::F10);
            if (refresh && key->key == LWS::KeyCode::F10 && (!fInterface.caption || !fInterface.menuBar) && restoreMenu)
                restoreMenu();
        }
        else if (const auto* mouse = std::get_if<LWS::EventMouseButton>(&event))
            refresh = mouse->pressed &&
                      fChromeHost->MainMenu()->Bounds().Contains(float(mouse->position.x), float(mouse->position.y));
        // This listener precedes UIHost: replace definitions before it begins input traversal.
        if (refresh)
            RefreshMenu();
    }

    void MainWindow::ApplyInterfacePreferences(const InterfacePreferences& preferences)
    {
        fInterface = preferences;
        if (fWindow.IsCreated() && !fWindow.GetParent())
            UpdateNativeWindowBorders(preferences.borders);
        if (fChromeHost && fChromeHost->MainMenu())
        {
            // Hiding the caption suppresses the whole title row without changing the menu preference.
            const bool caption = preferences.caption;
            auto* bar          = fChromeHost->MainMenu();
            bar->SetIcon(caption ? fApplicationIcon : nullptr);
            fTitleLabel->SetVisible(caption);
            bar->SetWindowControls({.minimize            = caption,
                                    .maximize            = caption,
                                    .draggable           = true,
                                    .requestClose        = caption ? requestClose : std::function<void()>{},
                                    .doubleClickMaximize = true});
            RefreshMenu();
        }
        if (fWindow.IsCreated())
            UpdateLayout();
    }

    bool MainWindow::GetShowImageControl() const
    {
        return fShowImageControl;
    }

    bool MainWindow::GetShowStatusBar() const
    {
        return fShowStatusBar &&
               (fWindow.GetWindowStyles() & LWS::WindowStyle::ResizableBorder) != LWS::WindowStyle::NoStyle;
    }

    void MainWindow::UpdateLayout()
    {
        LWS::LogicalSize canvasSize = fWindow.GetClientAreaMetrics().logical;
        const bool showChrome       = fChromeHost && fChromeHost->MainMenu() &&
                                GetWindowMode() == LWS::WindowMode::Windowed && fInterface.caption;
        const int32_t titleHeight = showChrome ? static_cast<int32_t>(fChromeHost->Style().menuBarHeight) : 0;
        if (fChromeHost && fChromeHost->MainMenu())
            fChromeHost->MainMenu()->SetVisible(showChrome);
        UpdateNativeStatusBar(canvasSize);
        const int32_t statusHeight = fWindow.GetClientAreaMetrics().logical.y - canvasSize.y;
        canvasSize.x               = std::max(canvasSize.x, 1);
        canvasSize.y               = std::max(canvasSize.y - titleHeight, 1);
        if (!fShowImageControl)
            std::ignore = fCanvasWindow.RequestPlacement(
                {.position = LWS::Point{0, titleHeight}, .clientSize = canvasSize});

        if (fSidebarLayout)
        {
            fImageControl->SetVisible(fShowImageControl);
            fSidebarSplit->SetVisible(fShowImageControl);
            // Reserve the remaining width for the canvas so the splitter cannot overgrow the sidebar.
            fSidebarSplit->SetMinimumSizes(
                std::max(100.f, float(canvasSize.x) - SidebarDividerWidth - SidebarMaximumWidth), 90);
            fSidebarLayout->padding = {0, 0, 0, float(statusHeight)};
            // Arrange immediately for hidden/embedded windows as well as normal UI frames.
            fSidebarLayout->Measure({float(canvasSize.x), float(canvasSize.y + statusHeight)});
            fSidebarLayout->Arrange({0, float(titleHeight), float(canvasSize.x), float(canvasSize.y + statusHeight)});
            fChromeHost->Invalidate(true);
        }
    }

    void MainWindow::ShowStatusBar(bool show)
    {
        if (show != fShowStatusBar)
        {
            fShowStatusBar = show;
            UpdateLayout();
        }
    }

    void MainWindow::SetShowImageControl(bool show)
    {
        if (show == fShowImageControl)
            return;

        fShowImageControl = show;
        UpdateLayout();
    }

    LWS::PixelSize MainWindow::GetCanvasPixelSize() const
    {
        const auto size = fCanvasWindow.GetClientAreaMetrics();
        return size.pixels.value_or(LWS::PixelSize{size.logical.x, size.logical.y});
    }

    LWS::Point MainWindow::GetCanvasMousePosition() const
    {
        const auto size               = fCanvasWindow.GetClientAreaMetrics();
        const LWS::Point position     = fCanvasWindow.GetMousePosition();
        const LWS::ContentScale scale = size.Scale().value_or(LWS::ContentScale{});
        return {static_cast<int32_t>(std::lround(position.x * scale.x)),
                static_cast<int32_t>(std::lround(position.y * scale.y))};
    }

    ImageControl& MainWindow::GetImageControl()
    {
        return *fImageControl;
    }

    LWS::Window& MainWindow::GetCanvasWindow()
    {
        return fCanvasWindow;
    }

    void MainWindow::ShowCanvas()
    {
        std::ignore = fCanvasWindow.SetVisible(true);
    }
}  // namespace OIV
