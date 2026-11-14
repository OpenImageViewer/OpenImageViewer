#pragma once
#include "ResizeDimensions.h"
#include <ImageProcessing/ImageResize.h>
#include <LLUtils/Rect.h>
#include <LWS/Bitmap.hpp>
#include <LWS/Window.hpp>
#include <LWSUI/Composites.hpp>
#include <filesystem>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace LWSUI
{
    class UIHost;
    class Control;
    class Window;
}  // namespace LWSUI

namespace OIV
{
    struct ResizeThumbwheelSettings
    {
        double startSpeed = 2, maximumSpeed = 4000, acceleration = .5;
        double deadZonePercent = 5, dragSensitivity = 1, fineModeMultiplier = .1;
    };

    struct ResizeFormDefaults
    {
        std::optional<ResizeFormDimensions> size;
        ResizeSizeMode sizeMode  = ResizeSizeMode::Absolute;
        ResizeUnits units        = ResizeUnits::Pixels;
        double dpi               = 96.0;
        ResizeAspectRatio aspect = ResizeAspectRatio::Preserve;
        bool autoFilter          = true;
        ResizeFilter filter      = ResizeFilter::Mitchell;
    };

    // Bounds are in the source image's pixel coordinates and define the dimensions used for resize math.
    struct ResizeFormInput
    {
        LWS::BitmapSharedPtr sourceBitmap;
        LWS::BitmapSharedPtr iconBitmap;
        LLUtils::RectI32 originalBounds;
        ResizeFormDefaults defaults;
        ResizeThumbwheelSettings thumbwheel;
        std::function<void(ImageResizeRequest)> onResize;
        std::function<void()> onDismiss;
    };

    class ResizePreview;

    class ResizeFormController
    {
      public:

        ResizeFormController(LWS::PlatformContext& platform, const std::filesystem::path& resources,
                             ResizeFormInput input = {});
        ~ResizeFormController();
        void Show(LWS::Window* owner);
        void Hide();
        void SetBusy(bool busy);
        void SetError(std::string message);
        void SetThumbwheelSettings(const ResizeThumbwheelSettings& settings);

      private:

        friend struct ResizeFormTestAccess;
        using SizeBasis = ResizeSizeBasis;
        using PixelSize = ResizeFormDimensions;

        LWS::PlatformContext& fPlatform;
        std::filesystem::path fResources;
        ResizeFormInput fInput;
        std::unique_ptr<LWS::Window> fDialogWindow;
        std::unique_ptr<LWSUI::UIHost> fUIHost;
        LWSUI::ConnectionSet fConnections;
        LWS::EventConnection fWindowEvents;
        LWSUI::RadioGroup* fSizeModeRadio{nullptr};
        LWSUI::RadioGroup* fUnitsRadio{nullptr};
        LWSUI::Panel* fUnitsPanel{nullptr};
        LWSUI::Label* fUnitsTitle{nullptr};
        LWSUI::Panel* fDPIBox{nullptr};
        LWSUI::ComboBox* fStandardResolutions{nullptr};
        LWSUI::TextBox* fWidthInput{nullptr};
        LWSUI::TextBox* fHeightInput{nullptr};
        LWSUI::TextBox* fDPIInput{nullptr};
        LWSUI::RadioGroup* fAspectRatioRadio{nullptr};
        LWSUI::Label* fWidthUnit{nullptr};
        LWSUI::Label* fHeightUnit{nullptr};
        LWSUI::Label* fPrintSize{nullptr};
        LWSUI::Label* fPreviewPixels{nullptr};
        LWSUI::Label* fPreviewCaption{nullptr};
        ResizePreview* fPreview{nullptr};
        LWSUI::Button* fApplyButton{nullptr};
        LWSUI::CheckBox* fAutoFilter{nullptr};
        LWSUI::ComboBox* fFilterCombo{nullptr};
        LWSUI::Label* fStatusLabel{nullptr};
        ResizeFilter fManualFilter = ResizeFilter::Mitchell;
        bool fBusy{};
        ResizeSizeMode fCurrentSizeMode = ResizeSizeMode::Absolute;
        ResizeUnits fCurrentUnits       = ResizeUnits::Pixels;
        SizeBasis fSizeBasis;
        // Immutable initialization baseline; wheel previews use a separate per-gesture snapshot.
        SizeBasis fInitialBasis;
        double fInitialDPI{};
        std::optional<PixelSize> fInitialPixels;
        std::optional<SizeBasis> fWheelBasis;
        double fWheelOffset{};
        std::array<LWSUI::Button*, 3> fResetButtons{};
        std::array<LWSUI::Thumbwheel*, 2> fThumbwheels{};
        double fOriginalWidth  = 400.0;
        double fOriginalHeight = 304.0;
        double fDPI            = 96.0;
        std::optional<SizeBasis> fDPIEditBasis;
        double fDPIEditStart{};
        LWS::LogicalSize fDialogSize{760, 500};
        std::string fDisplayedWidth;
        std::string fDisplayedHeight;
        std::optional<PixelSize> fDraftPixels;
        bool fDimensionDraft{};
        bool fDraftHeight{};
        bool fLastEditedHeight{};
        bool fUpdatingFields{};

        void ResetAll();
        void ResetDimension(bool height);
        bool fWheelHeight = false;
        void OnThumbwheel(bool height, double displacement, LWSUI::EditPhase phase);
        void AcceptDimensionBasis(const SizeBasis& basis, bool height);
        void BuildUI();
        void Apply();
        void Dismiss();
        void UpdateFilter(const ImageResizeLayout* layout);
        std::optional<ImageResizeRequest> CurrentRequest() const;
        std::optional<ImageResizeLayout> CurrentLayout() const;
        void SetSizeMode(ResizeSizeMode mode);
        void UpdateVisibility();
        void ResizeDialog();
        void OnSizeModeChanged(const std::string& value);
        void OnUnitsChanged(const std::string& value);
        void OnDPIChanged(const std::string& value, LWSUI::EditPhase phase);
        void OnStandardResolutionChanged(const std::string& value);
        void OnAspectRatioChanged();
        std::optional<double> CurrentDPI() const;
        std::optional<PixelSize> PixelsFromBasis() const;
        std::optional<PixelSize> CurrentPixels() const;
        void UpdateFieldsFromBasis();
        void OnDimensionChanged(bool height, const std::string& value, LWSUI::EditPhase phase);
        void UpdateCalculatedPixels();
    };
}  // namespace OIV
