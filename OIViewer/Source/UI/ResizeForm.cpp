#include "ResizeForm.h"
#include <LWSUI/UIHost.hpp>
#include <LWSUI/Containers.hpp>
#include <LWSUI/Primitives.hpp>
#include <LWSUI/Composites.hpp>
#include <LWSUI/Choice.hpp>
#include <LWSUI/Theme.hpp>
#include <LWS/Window.hpp>
#include <LWS/WindowTypes.hpp>
#include <LLUtils/StringUtility.h>
#include <algorithm>
#include <cmath>
#include <sstream>

namespace OIV
{

ResizeFormController::ResizeFormController(LWS::PlatformContext& platform, const std::filesystem::path& resources)
    : fPlatform(platform), fResources(resources)
{
}

ResizeFormController::~ResizeFormController()
{
    fUIHost.reset();
    fDialogWindow.reset();
}

void ResizeFormController::Show(LWS::Window* owner)
{
    if (!fDialogWindow)
    {
        LWS::WindowConfig config;
        config.clientSize = {650, 600};
        config.styles = LWS::WindowStyle::CloseButton | LWS::WindowStyle::ResizableBorder;
        config.backgroundColor = LLUtils::Color(244, 246, 248);
        fDialogWindow = std::make_unique<LWS::Window>(fPlatform);
        if (fDialogWindow->Create(config) != LWS::Result::Success)
        {
            fDialogWindow.reset();
            return;
        }
    }

    if (!fUIHost)
    {
        fUIHost = std::make_unique<LWSUI::UIHost>(*fDialogWindow, LWSUI::MakeTheme(LWSUI::ThemePreset::Light));
        fUIHost->SetRedrawOnResize(true);
        BuildUI();
    }

    fDialogWindow->SetTitle(LLUTILS_TEXT("Resize Image"));
    std::ignore = fDialogWindow->SetVisible(true);

    if (auto result = fDialogWindow->RequestShowState(LWS::WindowShowState::Restored); result != LWS::Result::Success)
    {
    }

    if (owner && fUIHost->Root() && fUIHost->Root()->Host())
    {
        auto& hostWindow = fUIHost->Root()->Host()->Window();
        auto placement = owner->GetPlacement();
        if (placement.position.has_value())
        {
            int x = placement.position->x + (placement.clientSize.x - 650) / 2;
            int y = placement.position->y + (placement.clientSize.y - 600) / 2;
            fDialogWindow->RequestPlacement({.position = LWS::Point{x, y},
                                             .clientSize = LWS::LogicalSize{650, 600}});
        }
    }
}

void ResizeFormController::BuildUI()
{
    auto root = std::make_unique<LWSUI::StackPanel>();
    root->spacing = 6;
    root->orientation = LWSUI::Orientation::Vertical;

    auto createSectionLabel = [&](const std::string& text) -> LWSUI::Label*
    {
        auto label = new LWSUI::Label(text);
        label->fontRole = LWSUI::Control::FontRole::Small;
        label->SetForeground(fUIHost->Style().accent);
        return label;
    };

    auto createMutedLabel = [&](const std::string& text) -> LWSUI::Label*
    {
        auto label = new LWSUI::Label(text);
        label->SetForeground(fUIHost->Style().muted);
        return label;
    };

    // === Row 1: Standard resolution preset ===
    root->Add(std::unique_ptr<LWSUI::Control>(createSectionLabel("Standard resolution preset")));

    std::vector<LWSUI::Choice> standardResolutions = {
        {"Current", "current"},
        {"640 x 480 (VGA)", "640x480"},
        {"800 x 600 (SVGA)", "800x600"},
        {"1024 x 768 (XGA)", "1024x768"},
        {"1280 x 720 (HD)", "1280x720"},
        {"1366 x 768", "1366x768"},
        {"1600 x 900 (HD+)", "1600x900"},
        {"1920 x 1080 (Full HD)", "1920x1080"},
        {"2560 x 1440 (QHD)", "2560x1440"},
        {"3840 x 2160 (4K UHD)", "3840x2160"},
        {"Custom...", "custom"}
    };

    fStandardResolutions = new LWSUI::ComboBox(standardResolutions);
    fStandardResolutions->SetValue("current");
    fStandardResolutions->OnChange.Connect([this](const std::string& value) { OnStandardResolutionChanged(value); });
    auto presetRow = new LWSUI::Grid();
    presetRow->columns = {-1};
    presetRow->spacing = 6;
    presetRow->Add(std::unique_ptr<LWSUI::Control>(fStandardResolutions));
    root->Add(std::unique_ptr<LWSUI::Control>(presetRow));

    // === Row 2: Size mode (radio buttons side by side) ===
    root->Add(std::unique_ptr<LWSUI::Control>(createSectionLabel("Size mode")));

    std::vector<LWSUI::Choice> sizeModes = {
        {"Absolute size", "absolute"},
        {"Relative to original", "relative"}
    };
    fSizeModeRadio = new LWSUI::RadioGroup(sizeModes);
    fSizeModeRadio->SetValue("absolute");
    fSizeModeRadio->orientation = LWSUI::Orientation::Horizontal;
    fSizeModeRadio->OnChange.Connect([this](const std::string& value) { OnSizeModeChanged(value); });
    root->Add(std::unique_ptr<LWSUI::Control>(fSizeModeRadio));

    // === Section: Original image ===
    root->Add(std::unique_ptr<LWSUI::Control>(createMutedLabel("Original image")));

    fOriginalImageLabel = new LWSUI::Label("400 × 304 px");
    fOriginalImageLabel->verticalAlignment = LWSUI::Alignment::End;
    auto originalRow = new LWSUI::Grid();
    originalRow->columns = {-1, 200};
    originalRow->spacing = 6;
    originalRow->Add(std::unique_ptr<LWSUI::Control>(createMutedLabel("")));
    originalRow->Add(std::unique_ptr<LWSUI::Control>(fOriginalImageLabel));
    root->Add(std::unique_ptr<LWSUI::Control>(originalRow));

    // === Section: Units ===
    root->Add(std::unique_ptr<LWSUI::Control>(createSectionLabel("Units")));

    std::vector<LWSUI::Choice> unitsChoices = {
        {"Pixels", "pixels"},
        {"Inches", "inches"},
        {"Centimeters", "centimeters"}
    };
    fUnitsRadio = new LWSUI::RadioGroup(unitsChoices);
    fUnitsRadio->SetValue("inches");
    fUnitsRadio->orientation = LWSUI::Orientation::Horizontal;
    fUnitsRadio->OnChange.Connect([this](const std::string& value) { OnUnitsChanged(value); });
    root->Add(std::unique_ptr<LWSUI::Control>(fUnitsRadio));

    // === Section: Output dimensions ===
    root->Add(std::unique_ptr<LWSUI::Control>(createSectionLabel("Output dimensions")));

    auto dimPanel = new LWSUI::Grid();
    dimPanel->columns = {-1, -1};
    dimPanel->spacing = 8;

    // Width box
    auto widthBox = new LWSUI::Panel();
    widthBox->paintBackground = true;
    auto widthStack = new LWSUI::StackPanel();
    widthStack->spacing = 2;

    auto widthLabel = new LWSUI::Label("WIDTH");
    widthLabel->SetForeground(fUIHost->Style().muted);
    widthStack->Add(std::unique_ptr<LWSUI::Control>(widthLabel));

    fWidthInput = new LWSUI::TextBox();
    fWidthInput->SetInputFilter(LWSUI::InputFilters::Number(false, true));
    fWidthInput->SetFontWeight(600);
    fWidthInput->OnEdit.Connect([this](const std::string& value, LWSUI::EditPhase phase) { OnWidthChanged(value, phase); });
    widthStack->Add(std::unique_ptr<LWSUI::Control>(fWidthInput));

    auto widthUnitLabel = new LWSUI::Label("in");
    widthUnitLabel->verticalAlignment = LWSUI::Alignment::End;
    widthUnitLabel->SetForeground(fUIHost->Style().muted);
    widthStack->Add(std::unique_ptr<LWSUI::Control>(widthUnitLabel));

    widthBox->SetContent(std::unique_ptr<LWSUI::Control>(widthStack));
    dimPanel->Add(std::unique_ptr<LWSUI::Control>(widthBox));

    // Height box
    auto heightBox = new LWSUI::Panel();
    heightBox->paintBackground = true;
    auto heightStack = new LWSUI::StackPanel();
    heightStack->spacing = 2;

    auto heightLabel = new LWSUI::Label("HEIGHT");
    heightLabel->SetForeground(fUIHost->Style().muted);
    heightStack->Add(std::unique_ptr<LWSUI::Control>(heightLabel));

    fHeightInput = new LWSUI::TextBox();
    fHeightInput->SetInputFilter(LWSUI::InputFilters::Number(false, true));
    fHeightInput->SetFontWeight(600);
    fHeightInput->OnEdit.Connect([this](const std::string& value, LWSUI::EditPhase phase) { OnHeightChanged(value, phase); });
    heightStack->Add(std::unique_ptr<LWSUI::Control>(fHeightInput));

    auto heightUnitLabel = new LWSUI::Label("in");
    heightUnitLabel->verticalAlignment = LWSUI::Alignment::End;
    heightUnitLabel->SetForeground(fUIHost->Style().muted);
    heightStack->Add(std::unique_ptr<LWSUI::Control>(heightUnitLabel));

    heightBox->SetContent(std::unique_ptr<LWSUI::Control>(heightStack));
    dimPanel->Add(std::unique_ptr<LWSUI::Control>(heightBox));

    root->Add(std::unique_ptr<LWSUI::Control>(dimPanel));

    // === Section: Aspect ratio ===
    root->Add(std::unique_ptr<LWSUI::Control>(createSectionLabel("Aspect ratio")));

    std::vector<LWSUI::Choice> aspectRatios = {
        {"Preserve original ratio", "preserve"},
        {"Actual pixels", "none"},
        {"Black bars", "preserve_bars"}
    };
    fAspectRatioRadio = new LWSUI::RadioGroup(aspectRatios);
    fAspectRatioRadio->SetValue("preserve");
    fAspectRatioRadio->orientation = LWSUI::Orientation::Horizontal;
    fAspectRatioRadio->OnChange.Connect([this](const std::string& value) { OnAspectRatioChanged(value); });
    root->Add(std::unique_ptr<LWSUI::Control>(fAspectRatioRadio));

    // === Section: Resolution ===
    root->Add(std::unique_ptr<LWSUI::Control>(createSectionLabel("Resolution")));

    auto resPanel = new LWSUI::Grid();
    resPanel->columns = {200, -1};
    resPanel->spacing = 8;

    // DPI box (left)
    auto dpiBox = new LWSUI::Panel();
    dpiBox->paintBackground = true;
    auto dpiStack = new LWSUI::StackPanel();
    dpiStack->spacing = 2;

    auto dpiLabel = new LWSUI::Label("DPI");
    dpiLabel->SetForeground(fUIHost->Style().muted);
    dpiStack->Add(std::unique_ptr<LWSUI::Control>(dpiLabel));

    fDPIInput = new LWSUI::TextBox();
    fDPIInput->SetInputFilter(LWSUI::InputFilters::Number(false, true));
    fDPIInput->SetFontWeight(600);
    fDPIInput->SetText("96");
    dpiStack->Add(std::unique_ptr<LWSUI::Control>(fDPIInput));

    auto dpiUnitLabel = new LWSUI::Label("dots/in");
    dpiUnitLabel->verticalAlignment = LWSUI::Alignment::End;
    dpiUnitLabel->SetForeground(fUIHost->Style().muted);
    dpiStack->Add(std::unique_ptr<LWSUI::Control>(dpiUnitLabel));

    dpiBox->SetContent(std::unique_ptr<LWSUI::Control>(dpiStack));
    resPanel->Add(std::unique_ptr<LWSUI::Control>(dpiBox));

    // Calculated output (right)
    auto calcBox = new LWSUI::Panel();
    calcBox->paintBackground = true;
    auto calcStack = new LWSUI::StackPanel();
    calcStack->spacing = 2;

    auto calcTitleLabel = new LWSUI::Label("CALCULATED OUTPUT IN PIXELS");
    calcTitleLabel->SetForeground(fUIHost->Style().muted);
    calcStack->Add(std::unique_ptr<LWSUI::Control>(calcTitleLabel));

    fCalculatedPixelsLabel = new LWSUI::Label("1920 × 1080");
    fCalculatedPixelsLabel->SetForeground(fUIHost->Style().accent);
    calcStack->Add(std::unique_ptr<LWSUI::Control>(fCalculatedPixelsLabel));

    calcBox->SetContent(std::unique_ptr<LWSUI::Control>(calcStack));
    resPanel->Add(std::unique_ptr<LWSUI::Control>(calcBox));

    root->Add(std::unique_ptr<LWSUI::Control>(resPanel));

    // Formula text below resolution
    fCalcHintLabel = new LWSUI::Label("5.33 in × 300 DPI = 1600 px   ·   4.05 in × 300 DPI = 1216 px");
    fCalcHintLabel->SetForeground(fUIHost->Style().muted);
    root->Add(std::unique_ptr<LWSUI::Control>(fCalcHintLabel));

    // === Footer ===
    auto buttonPanel = new LWSUI::FlowPanel();
    buttonPanel->gap = 6;
    buttonPanel->alignEnd = true;

    fCancelButton = new LWSUI::Button("Cancel");
    fCancelButton->OnClick.Connect([this]() { std::ignore = fDialogWindow->SetVisible(false); });
    buttonPanel->AddItem(std::unique_ptr<LWSUI::Control>(fCancelButton));

    fApplyButton = new LWSUI::Button("Resize image");
    fApplyButton->OnClick.Connect([this]()
    {
    });
    buttonPanel->AddItem(std::unique_ptr<LWSUI::Control>(fApplyButton));

    root->Add(std::unique_ptr<LWSUI::Control>(buttonPanel));

    fRootControl = std::move(root);
    auto paddedRoot = new LWSUI::Panel(std::move(fRootControl));
    paddedRoot->paintBackground = false;
    paddedRoot->padding = {12, 12, 12, 12};
    fUIHost->SetRoot(std::unique_ptr<LWSUI::Control>(paddedRoot));

    UpdateVisibility();
    UpdateCalculatedPixels();
}

void ResizeFormController::UpdateVisibility()
{
    bool showUnits = (fCurrentSizeMode == SizeMode::Absolute);
    bool showDPI = (fCurrentSizeMode == SizeMode::Absolute && fCurrentUnits != Units::Pixels);

    if (fUnitsRadio) fUnitsRadio->SetVisible(showUnits);
    if (fDPIInput) fDPIInput->SetVisible(showDPI);
    if (fCalcHintLabel) fCalcHintLabel->SetVisible(showDPI);
    if (fCalculatedPixelsLabel) fCalculatedPixelsLabel->SetVisible(showDPI);

    if (fUIHost && fUIHost->Root())
    {
        fUIHost->Invalidate(true);
    }
}

void ResizeFormController::OnSizeModeChanged(const std::string& value)
{
    if (value == "relative")
    {
        SetSizeMode(SizeMode::Relative);
    }
    else if (value == "absolute")
    {
        SetSizeMode(SizeMode::Absolute);
    }
}

void ResizeFormController::SetSizeMode(SizeMode mode)
{
    fCurrentSizeMode = mode;
    UpdateVisibility();

    if (mode == SizeMode::Relative)
    {
        fWidthInput->SetText("100");
        fHeightInput->SetText("100");
    }
    else if (mode == SizeMode::Absolute)
    {
        fWidthInput->SetText(std::to_string(static_cast<int>(fOriginalWidth)));
        fHeightInput->SetText(std::to_string(static_cast<int>(fOriginalHeight)));
    }

    UpdateCalculatedPixels();
}

void ResizeFormController::OnUnitsChanged(const std::string& value)
{
    if (value == "pixels")
    {
        fCurrentUnits = Units::Pixels;
    }
    else if (value == "inches")
    {
        fCurrentUnits = Units::Inches;
    }
    else if (value == "centimeters")
    {
        fCurrentUnits = Units::Centimeters;
    }
    UpdateVisibility();
    UpdateCalculatedPixels();
}

void ResizeFormController::OnStandardResolutionChanged(const std::string& value)
{
    if (value == "current")
    {
    }
    else if (value == "custom")
    {
        fSizeModeRadio->SetValue("absolute");
        fUnitsRadio->SetValue("pixels");
    }
    else
    {
        size_t pos = value.find('x');
        if (pos != std::string::npos)
        {
            int width = std::stoi(value.substr(0, pos));
            int height = std::stoi(value.substr(pos + 1));
            fOriginalWidth = width;
            fOriginalHeight = height;
            fSizeModeRadio->SetValue("absolute");
            fUnitsRadio->SetValue("pixels");
            fWidthInput->SetText(std::to_string(width));
            fHeightInput->SetText(std::to_string(height));
            UpdateCalculatedPixels();
        }
    }
}

void ResizeFormController::OnAspectRatioChanged(const std::string& value)
{
    UpdateCalculatedPixels();
}

void ResizeFormController::OnWidthChanged(const std::string& value, LWSUI::EditPhase phase)
{
    if (phase != LWSUI::EditPhase::Commit && phase != LWSUI::EditPhase::Preview)
        return;

    fStandardResolutions->SetValue("custom");

    if (fAspectRatioRadio && fAspectRatioRadio->Value() == "preserve")
    {
        double width = 0;
        try { width = std::stod(value); } catch (...) { return; }
        if (width <= 0) return;

        double ratio = fOriginalHeight / fOriginalWidth;
        double newHeight = width * ratio;
        fHeightInput->SetText(std::to_string(newHeight));
    }
    UpdateCalculatedPixels();
}

void ResizeFormController::OnHeightChanged(const std::string& value, LWSUI::EditPhase phase)
{
    if (phase != LWSUI::EditPhase::Commit && phase != LWSUI::EditPhase::Preview)
        return;

    fStandardResolutions->SetValue("custom");

    if (fAspectRatioRadio && fAspectRatioRadio->Value() == "preserve")
    {
        double height = 0;
        try { height = std::stod(value); } catch (...) { return; }
        if (height <= 0) return;

        double ratio = fOriginalWidth / fOriginalHeight;
        double newWidth = height * ratio;
        fWidthInput->SetText(std::to_string(newWidth));
    }
    UpdateCalculatedPixels();
}

void ResizeFormController::UpdateCalculatedPixels()
{
    double outWidth = 0, outHeight = 0;

    try
    {
        double width = std::stod(fWidthInput->Text());
        double height = std::stod(fHeightInput->Text());

        switch (fCurrentSizeMode)
        {
            case SizeMode::Relative:
                outWidth = fOriginalWidth * (width / 100.0);
                outHeight = fOriginalHeight * (height / 100.0);
                break;
            case SizeMode::Absolute:
                switch (fCurrentUnits)
                {
                    case Units::Pixels:
                        outWidth = width;
                        outHeight = height;
                        break;
                    case Units::Inches:
                        outWidth = width * std::stod(fDPIInput->Text());
                        outHeight = height * std::stod(fDPIInput->Text());
                        break;
                    case Units::Centimeters:
                        outWidth = (width / 2.54) * std::stod(fDPIInput->Text());
                        outHeight = (height / 2.54) * std::stod(fDPIInput->Text());
                        break;
                }
                break;
        }
    }
    catch (...)
    {
        outWidth = fOriginalWidth;
        outHeight = fOriginalHeight;
    }

    std::ostringstream oss;
    oss << static_cast<int>(std::round(outWidth)) << " × " << static_cast<int>(std::round(outHeight));
    if (fCalculatedPixelsLabel)
    {
        fCalculatedPixelsLabel->SetText(oss.str());
    }
}

} // namespace OIV
