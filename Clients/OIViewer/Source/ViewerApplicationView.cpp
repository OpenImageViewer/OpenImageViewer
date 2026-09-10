#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "ViewerApplication.h"

#include <Version.h>

#include <Functions.h>
#include <ApiGlobal.h>

#include <LLUtils/Exception.h>
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/FileSystemHelper.h>
#include <LLUtils/Rect.h>

#include <OIVAppCore/OIVHelper.h>
#include "Helpers/ClipboardSetup.h"
#include <OIVAppCore/MessageHelper.h>
#include <OIVAppCore/ShellIntegrationHelper.h>
#include "Helpers/ShellCommandHandler.h"

#include "OIVCommands.h"

#include "OIVImage/OIVFileImage.h"
#include "OIVImage/OIVRawImage.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "Globals.h"
#include <OIVAppCore/ConfigurationLoader.h>
#include "CommandRegistry.h"
#include <OIVAppCore/ColorCountPolicy.h>
#include <OIVAppCore/ColorCorrectionCommandPolicy.h>
#include <OIVAppCore/FileChangePolicy.h>
#include <OIVAppCore/FrameLimiterPolicy.h>
#include <OIVAppCore/ImageEditPolicy.h>
#include <OIVAppCore/ImageFormatCatalogPolicy.h>
#include <OIVAppCore/ImageLoadPresentationPolicy.h>
#include <OIVAppCore/ImageTransformCommandPolicy.h>
#include <OIVAppCore/InputGesturePolicy.h>
#include <OIVAppCore/OIVImageHelper.h>
#include <OIVAppCore/SelectionWorkflowPolicy.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVAppCore/SortCommandPolicy.h>
#include <OIVAppCore/SubImagePolicy.h>
#include <OIVAppCore/ViewActionController.h>
#include <OIVAppCore/ViewCommandPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <OIVShared/PixelHelper.h>
#include <ImageUtil/ImageUtil.h>
#include "InterThreadMessages.h"

namespace OIV
{
    void ViewerApplication::OnSelectionRectChanged(const LLUtils::RectI32& selectionRect, bool isVisible)
    {
        if (isVisible)
            fRenderGateway->SetSelectionRect(selectionRect);
        else
            fRenderGateway->ClearSelectionRect();

        fRefreshOperation.Queue();
    }

    // callback from queued operation

    void ViewerApplication::OnScroll(const LLUtils::PointF64& panAmount)
    {
        Pan(panAmount);

        const PanCursorHint cursorHint          = InputGesturePolicy::CursorHintForPan(panAmount);
        const MainWindow::CursorType cursorType = cursorHint.sizeAll ? MainWindow::CursorType::SizeAll
                                                                     : static_cast<MainWindow::CursorType>(
                                                                           cursorHint.directionIndex + 2);
        fWindow.SetCursorType(cursorType);
    }

    void ViewerApplication::DelayResamplingCallback()
    {
        fTimerNoActiveZoom.SetInterval(0);
        fImageState.SetResample(true);
        fImageState.Refresh();
        fRefreshOperation.Queue();
    }

    OIV_Filter_type ViewerApplication::GetFilterType() const
    {
        return fImageState.GetVisibleImage()->GetFilterType();
    }

    void ViewerApplication::UpdateExposure()
    {
        fRenderGateway->SetColorExposure(fColorExposure);
        fRefreshOperation.Queue();
    }

    bool ViewerApplication::ToggleColorCorrection()
    {
        bool isDefault = true;
        if (memcmp(&fColorExposure, &DefaultColorCorrection, sizeof(OIV_CMD_ColorExposure_Request)) == 0)
        {
            std::swap(fLastColorExposure, fColorExposure);
        }
        else
        {
            isDefault = false;
            std::swap(fLastColorExposure, fColorExposure);
            fColorExposure = DefaultColorCorrection;
        }
        UpdateExposure();

        return isDefault;
    }

    void ViewerApplication::ToggleFullScreen(bool multiFullScreen)
    {
        fRefreshOperation.Begin();
        const LWS::WindowMode currentMode   = fWindow.GetWindow().GetWindowMode();
        const LWS::WindowMode requestedMode = currentMode == LWS::WindowMode::Windowed
                                                  ? (multiFullScreen ? LWS::WindowMode::FullscreenAllMonitors
                                                                     : LWS::WindowMode::Fullscreen)
                                                  : LWS::WindowMode::Windowed;
        std::ignore                         = fWindow.GetWindow().SetWindowMode(requestedMode);
        Center();
        fRefreshOperation.End();
    }

    void ViewerApplication::ToggleBorders()
    {
        fShowBorders = !fShowBorders;
        {
            std::ignore = fWindow.GetWindow().SetWindowStyles(fShowBorders
                                                                  ? LWS::WindowStyleFlags(WindowChromeStyles)
                                                                  : LWS::WindowStyleFlags(LWS::WindowStyle::NoStyle));
            fWindow.UpdateLayout();
        }
    }

    void ViewerApplication::SetSlideShowEnabled(bool enabled)
    {
        if (fSlideshowPolicy.IsEnabled() != enabled)
        {
            fSlideshowPolicy.SetEnabled(enabled);
            fTimerSlideShow.SetInterval(fSlideshowPolicy.GetTimerIntervalMs());
        }
    }

    void ViewerApplication::SetFilterLevel(OIV_Filter_type filterType)
    {
        fImageState.GetVisibleImage()->SetFilterType(
            std::clamp(filterType, FT_None, static_cast<OIV_Filter_type>(FT_Count - 1)));

        fRefreshOperation.Queue();
    }

    void ViewerApplication::ToggleGrid()
    {
        fIsGridEnabled = !fIsGridEnabled;
        UpdateRenderViewParams();
    }

    void ViewerApplication::UpdateRenderViewParams()
    {
        CmdRequestTexelGrid grid;
        grid.gridSize         = fIsGridEnabled ? 1.0 : 0.0;
        grid.transparencyMode = fTransparencyMode;
        grid.generateMipmaps  = fDownScalingTechnique == DownscalingTechnique::HardwareMipmaps;
        if (fRenderGateway->SetTexelGrid(grid) == RC_Success)
        {
            fRefreshOperation.Queue();
        }
    }

    void ViewerApplication::SetOffset(LLUtils::PointF64 offset, bool preserveOffsetLockState)
    {
        fImageState.SetOffset(ResolveOffset(offset));
        fPreserveImageSpaceSelection.Queue();
        fRefreshOperation.Queue();

        if (preserveOffsetLockState == false)
            fIsOffsetLocked = false;
    }

    void ViewerApplication::SetOriginalSize()
    {
        SetZoomInternal(1.0, -1, -1);
        Center();
    }

    void ViewerApplication::Pan(const LLUtils::PointF64& panAmount)
    {
        if (fImageState.GetOpenedImage() != nullptr)
            SetOffset(panAmount * fDPIadjustmentFactor + fImageState.GetOffset());
    }

    void ViewerApplication::Zoom(double amount, int zoomX, int zoomY)
    {
        if (IsImageOpen())
        {
            CommandManager::CommandRequest request;
            request.displayName = "Zoom";
            request.args        = CommandManager::CommandArgs::FromString(
                "val=" + std::to_string(amount) + ";cx=" + std::to_string(zoomX) + ";cy=" + std::to_string(zoomY));
            request.commandName = "cmd_zoom";
            ExecuteCommand(request);
        }
    }

    void ViewerApplication::ZoomInternal(double amount, int zoomX, int zoomY)
    {
        const double adaptiveAmount = fAdaptiveZoom.Add(amount);
        const double adjustedAmount = ViewActionController::RelativeZoom(GetScale(), adaptiveAmount);
        SetZoomInternal(adjustedAmount, zoomX, zoomY);
    }

    void ViewerApplication::FitToClientAreaAndCenter()
    {
        if (IsImageOpen())
        {
            using namespace LLUtils;
            const LWS::PixelSize clientSize = fWindow.GetCanvasPixelSize();
            if (clientSize.x > 0 && clientSize.y > 0)  // window might minimized.
            {
                const double zoom = ViewTransformController::FitScale(PointF64(clientSize.x, clientSize.y),
                                                                      GetImageSize(ImageSizeType::Transformed));
                fRefreshOperation.Begin();
                fIsLockFitToScreen = true;
                SetZoomInternal(zoom, -1, -1, true);
                Center();
                fRefreshOperation.End();
            }
        }
    }

    void ViewerApplication::UpdateSelectionRectText()
    {
        OIVTextImage* selectionSizeText = fLabelManager.GetTextLabel("selectionSizeText");
        auto selectionSizeStr           = SelectionWorkflowPolicy::FormatSelectionSize(fImageSpaceSelection);

        if (selectionSizeText == nullptr)
        {
            // Create new user message.
            selectionSizeText = fLabelManager.GetOrCreateTextLabel("selectionSizeText");
            selectionSizeText->SetFontPath(LabelManager::sFontPath);
            selectionSizeText->SetFontSize(11);
            selectionSizeText->SetBackgroundColor({0, 0, 0, 192});
            selectionSizeText->SetTextColor({170, 170, 170, 255});
            selectionSizeText->SetTextRenderMode(FreeType::RenderMode::Antialiased);
            selectionSizeText->SetOutlineWidth(0);

            selectionSizeText->SetFilterType(OIV_Filter_type::FT_None);
            selectionSizeText->SetImageRenderMode(OIV_Image_Render_mode::IRM_Overlay);
            selectionSizeText->SetScale({1.0, 1.0});
            selectionSizeText->SetOpacity(1.0);
        }

        // text already exists, just make visible.
        selectionSizeText->SetVisible(true);
        selectionSizeText->SetText(selectionSizeStr);
        selectionSizeText->Create();

        const auto labelPosition = SelectionWorkflowPolicy::PlaceSelectionLabel(
            fSelectionRect.GetSelectionRect(),
            {static_cast<int32_t>(selectionSizeText->GetImage()->GetWidth()),
             static_cast<int32_t>(selectionSizeText->GetImage()->GetHeight())},
            static_cast<LLUtils::PointI32>(fWindow.GetCanvasPixelSize()));

        selectionSizeText->SetPosition(static_cast<LLUtils::PointF64>(labelPosition));
    }

    void ViewerApplication::SetImageSpaceSelection(const LLUtils::RectI32& rect)
    {
        fImageSpaceSelection = rect;
        UpdateSelectionRectText();
    }

    void ViewerApplication::SaveImageSpaceSelection()
    {
        if (SelectionWorkflowPolicy::ShouldSaveImageSpaceSelection(fSelectionRect.GetOperation()))
            SetImageSpaceSelection(ClientToImageRounded(fSelectionRect.GetSelectionRect()));
    }

    void ViewerApplication::LoadImageSpaceSelection()
    {
        if (fImageSpaceSelection.IsEmpty() == false)
        {
            LLUtils::RectI32 r = static_cast<LLUtils::RectI32>(
                ImageToClient(static_cast<LLUtils::RectF64>(fImageSpaceSelection)));
            fSelectionRect.UpdateSelection(r);
            UpdateSelectionRectText();
        }
    }

    void ViewerApplication::CancelSelection()
    {
        OIVTextImage* selectionSizeText = fLabelManager.GetTextLabel("selectionSizeText");
        if (selectionSizeText != nullptr)
            selectionSizeText->SetVisible(false);

        fSelectionRect.SetSelection(SelectionRect::Operation::CancelSelection, {0, 0});
        fImageSpaceSelection = decltype(fImageSpaceSelection)::Zero;
    }

    double ViewerApplication::GetMinimumPixelSize()
    {
        return ViewActionController::MinimumPixelSize(fMinImageSize, GetImageSize(ImageSizeType::Transformed));
    }

    void ViewerApplication::QueueResampling()
    {
        if (GetResamplingEnabled() && IsImageOpen() && fDownScalingTechnique == DownscalingTechnique::Software)
        {
            fImageState.SetResample(false);
            fTimerNoActiveZoom.SetInterval(0);
            fTimerNoActiveZoom.SetInterval(fQueueResamplingDelay);
        }
    }

    void ViewerApplication::SetResamplingEnabled(bool enable)
    {
        if (fIsResamplingEnabled != enable)
        {
            fIsResamplingEnabled = enable;
            if (fIsResamplingEnabled == false)
            {
                fTimerNoActiveZoom.SetInterval(0);
                fImageState.SetResample(false);
            }
            else
            {
                QueueResampling();
            }
        }
    }

    bool ViewerApplication::GetResamplingEnabled() const
    {
        return fIsResamplingEnabled;
    }

    void ViewerApplication::SetZoomInternal(double zoomValue, int clientX, int clientY, bool preserveFitToScreenState)
    {
        using namespace LLUtils;

        // Apply zoom limits only if zoom is not bound to the client window
        if (fIsLockFitToScreen == false)
        {
            // We want to keep the image at least the size of 'MinImagePixelsInSmallAxis' pixels in the smallest axis.
            zoomValue = ViewActionController::ResolveZoomValue(zoomValue, fIsLockFitToScreen, GetMinimumPixelSize(),
                                                               fMaxPixelSize);
        }

        if (zoomValue != fImageState.GetScale().x)
        {
            // Save image selection before view change
            fPreserveImageSpaceSelection.Begin();

            const PointI32 clientZoomPoint = ViewActionController::ResolveZoomPoint({clientX, clientY},
                                                                                    GetCanvasCenter());

            PointF64 imageZoomPoint = ClientToImage(clientZoomPoint);
            PointF64 offset         = ViewTransformController::ZoomOffset(imageZoomPoint, GetScale(), zoomValue);

            QueueResampling();

            fImageState.SetScale(zoomValue);

            fRefreshOperation.Begin();

            RefreshImage();

            // preserve offset lock (image centering) if zoom is realtive to the center of the image
            SetOffset(GetOffset() + offset, ViewActionController::ShouldPreserveOffsetLockForZoom(clientX, clientY));
            fPreserveImageSpaceSelection.End();

            fRefreshOperation.End();

            /*UpdateCanvasSize();
            UpdateUIZoom();*/

            if (preserveFitToScreenState == false)
                fIsLockFitToScreen = false;
        }
    }

    double ViewerApplication::GetScale() const
    {
        return fImageState.GetScale().x;
    }

    /*void ViewerApplication::UpdateCanvasSize()
    {
        if (fImageState.GetImage(ImageChainStage::Deformed) != nullptr)
        {
            using namespace  LLUtils;
            const LWS::PixelSize pixels = fWindow.GetCanvasPixelSize();
            PointF64 canvasSize =
    PointF64(pixels.x, pixels.y) / GetScale();
            LLUtils::native_stringstream ss; ss << LLUTILS_TEXT("Canvas:
    ")
                << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << canvasSize.x
                << LLUTILS_TEXT(" X ")
                << std::fixed << std::setprecision(1) << std::setfill(L' ') << std::setw(6) << canvasSize.y;
            fWindow.SetStatusBarText(ss.str(), 3, 0);
        }
    }*/

    void ViewerApplication::UpdateTexelPos()
    {
        if (fVirtualStatusBar.GetVisible() == true)
        {
            if (fImageState.GetImage(ImageChainStage::Deformed) != nullptr)
            {
                using namespace LLUtils;
                PointF64 storageImageSpace = ClientToImage(fWindow.GetWindowMousePosition());

                LLUtils::native_stringstream ss;
                ss << LLUTILS_TEXT("Texel: ") << std::fixed << std::setprecision(1)
                   << std::setfill(LLUTILS_TEXT(" ")[0]) << std::setw(6) << storageImageSpace.x << LLUTILS_TEXT(" X ")
                   << std::fixed << std::setprecision(1) << std::setfill(LLUTILS_TEXT(" ")[0]) << std::setw(6)
                   << storageImageSpace.y;
                fVirtualStatusBar.SetText("texelPos", ss.str());

                // fWindow.SetStatusBarText(ss.str(), 2, 0);

                PointF64 storageImageSize = GetImageSize(ImageSizeType::Transformed);

                if (!(storageImageSpace.x < 0 || storageImageSpace.y < 0 || storageImageSpace.x >= storageImageSize.x ||
                      storageImageSpace.y >= storageImageSize.y))
                {
                    LLUtils::native_string_type message = StringUtility::ToNativeString(
                        OIVHelper::ParseTexelValue(fImageState.GetImage(ImageChainStage::Deformed)->GetImage(),
                                                   static_cast<LLUtils::PointI32>(storageImageSpace)));
                    fVirtualStatusBar.SetText("texelValue", message);
                    fVirtualStatusBar.SetOpacity("texelValue", 1.0);
                    fRefreshOperation.Queue();
                }
                else
                {
                    if (fVirtualStatusBar.SetOpacity("texelValue", 0))
                        fRefreshOperation.Queue();
                }
            }
        }
    }

    void ViewerApplication::AutoPlaceImage(bool forceCenter)
    {
        fRefreshOperation.Begin();
        if (ViewActionController::ShouldFitToScreenOnAutoPlace(fIsLockFitToScreen, fIsOffsetLocked))
            FitToClientAreaAndCenter();
        else if (ViewActionController::ShouldCenterOnAutoPlace(fIsLockFitToScreen, fIsOffsetLocked, forceCenter))
            Center();
        fRefreshOperation.End();
    }

    void ViewerApplication::UpdateWindowSize()
    {
        const auto clientArea     = fWindow.GetCanvasWindow().GetClientAreaSize();
        const LWS::PixelSize size = clientArea.value_or(LWS::ClientAreaSize{}).pixels;
        fRenderGateway->SetViewportSize(size);
        if (size.x > 0 && size.y > 0)
        {
            const LWS::ContentScale scale = clientArea->Scale();
            fDPIadjustmentFactor          = {scale.x, scale.y};
            fLabelManager.SetContentScale(scale);
            AutoPlaceImage();
            const LLUtils::PointI32 point{size.x, size.y};
            fVirtualStatusBar.ClientSizeChanged(point);

            EventManager::GetSingleton().SizeChange.Raise(
                EventManager::SizeChangeEventParams{static_cast<int32_t>(size.x), static_cast<int32_t>(size.y)});
        }
    }

    void ViewerApplication::Center()
    {
        if (IsImageOpen() == true)
        {
            fRefreshOperation.Begin();
            using namespace LLUtils;
            PointF64 offset = ViewTransformController::CenterOffset(GetCanvasCenter(),
                                                                    GetImageSize(ImageSizeType::Visible));
            // Lock offset when centering
            fIsOffsetLocked = true;
            SetOffset(offset, true);
            fRefreshOperation.End();
        }
    }

    void ViewerApplication::TransformImage(IMUtil::AxisAlignedRotation relativeRotation, IMUtil::AxisAlignedFlip flip)
    {
        fRefreshOperation.Begin();
        SetResamplingEnabled(false);
        fImageState.Transform(relativeRotation, flip);
        AutoPlaceImage(true);
        RefreshImage();
        fRefreshOperation.End();
        SetResamplingEnabled(true);
    }

    void ViewerApplication::SetDownScalingTechnique(DownscalingTechnique technique)
    {
        if (technique != fDownScalingTechnique)
        {
            fDownScalingTechnique = technique;
            switch (fDownScalingTechnique)
            {
                case DownscalingTechnique::None:
                    fImageState.SetResample(false);
                    break;
                case DownscalingTechnique::HardwareMipmaps:
                    fImageState.SetResample(false);
                    break;
                case DownscalingTechnique::Software:
                    fImageState.SetResample(true);
                    break;
                default:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
            }

            if (IsImageOpen() == true)
            {
                fRefreshOperation.Begin();
                RefreshImage();
                UpdateRenderViewParams();
                fRefreshOperation.End();
            }
        }
    }
}  // namespace OIV
