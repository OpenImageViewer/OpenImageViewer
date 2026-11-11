#pragma once
#include <LWS/Window.hpp>
#include <LWSUI/Composites.hpp>
#include <memory>
#include <string>
#include <filesystem>

namespace LWSUI { class UIHost; class Control; class Window; }

namespace OIV
{

class ResizeFormController
{
public:
    ResizeFormController(LWS::PlatformContext& platform, const std::filesystem::path& resources);
    ~ResizeFormController();
    void Show(LWS::Window* owner);
private:
    LWS::PlatformContext& fPlatform;
    std::filesystem::path fResources;
    std::unique_ptr<LWS::Window> fDialogWindow;
    std::unique_ptr<LWSUI::UIHost> fUIHost;
    std::unique_ptr<LWSUI::Control> fRootControl;
    LWSUI::RadioGroup* fSizeModeRadio{nullptr};
    LWSUI::RadioGroup* fUnitsRadio{nullptr};
    LWSUI::ComboBox* fStandardResolutions{nullptr};
    LWSUI::TextBox* fWidthInput{nullptr};
    LWSUI::TextBox* fHeightInput{nullptr};
    LWSUI::TextBox* fDPIInput{nullptr};
    LWSUI::Label* fOriginalImageLabel{nullptr};
    LWSUI::Label* fCalculatedPixelsLabel{nullptr};
    LWSUI::Label* fCalcHintLabel{nullptr};
    LWSUI::RadioGroup* fAspectRatioRadio{nullptr};
    LWSUI::Button* fApplyButton{nullptr};
    LWSUI::Button* fCancelButton{nullptr};
    enum class SizeMode { Absolute, Relative };
    SizeMode fCurrentSizeMode = SizeMode::Absolute;
    enum class Units { Pixels, Inches, Centimeters };
    Units fCurrentUnits = Units::Inches;
    double fOriginalWidth = 400.0;
    double fOriginalHeight = 304.0;
    double fOriginalDPI = 96.0;
    void BuildUI();
    void SetSizeMode(SizeMode mode);
    void UpdateVisibility();
    void OnSizeModeChanged(const std::string& value);
    void OnUnitsChanged(const std::string& value);
    void OnStandardResolutionChanged(const std::string& value);
    void OnAspectRatioChanged(const std::string& value);
    void OnWidthChanged(const std::string& value, LWSUI::EditPhase phase);
    void OnHeightChanged(const std::string& value, LWSUI::EditPhase phase);
    void UpdateCalculatedPixels();
};

} // namespace OIV
