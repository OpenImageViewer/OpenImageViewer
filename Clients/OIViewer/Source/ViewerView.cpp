#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "Viewer.h"

#include <Version.h>

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

#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "Globals.h"
#include <OIVAppCore/ConfigurationLoader.h>
#include "CommandRegistry.h"
#include <OIVAppCore/ColorCorrectionCommandPolicy.h>
#include <OIVAppCore/FileChangePolicy.h>
#include <OIVAppCore/FrameLimiterPolicy.h>
#include <OIVAppCore/ImageEditPolicy.h>
#include <OIVAppCore/ImageFormatCatalogPolicy.h>
#include <OIVAppCore/ImageLoadPresentationPolicy.h>
#include <OIVAppCore/ImageTransformCommandPolicy.h>
#include <OIVAppCore/InputGesturePolicy.h>
#include <OIVAppCore/SelectionWorkflowPolicy.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVAppCore/SortCommandPolicy.h>
#include <OIVAppCore/SubImagePolicy.h>
#include <OIVAppCore/ViewActionController.h>
#include <OIVAppCore/ViewCommandPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <ImageUtil/ImageUtil.h>
#include <OIVAppCore/ViewerCompletion.h>

namespace OIV
{
    void Viewer::OnSelectionRectChanged(const LLUtils::RectI32& selectionRect, bool isVisible)
    {
        if (isVisible)
            fRenderer->SetSelectionRect(
                {selectionRect.GetCorner(LLUtils::TopLeft), selectionRect.GetCorner(LLUtils::BottomRight)});
        else
            fRenderer->SetSelectionRect({{-1, -1}, {-1, -1}});

        fRefreshOperation.Queue();
    }

    // callback from queued operation

    void Viewer::OnScroll(const LLUtils::PointF64& panAmount)
    {
        Pan(panAmount);

        const PanCursorHint cursorHint          = InputGesturePolicy::CursorHintForPan(panAmount);
        const MainWindow::CursorType cursorType = cursorHint.sizeAll ? MainWindow::CursorType::SizeAll
                                                                     : static_cast<MainWindow::CursorType>(
                                                                           cursorHint.directionIndex + 2);
        fWindow.SetCursorType(cursorType);
    }

    void Viewer::DelayResamplingCallback()
    {
        fTimerNoActiveZoom.SetInterval(0);
        fSession.fImageState.SetResample(fSession.fDownScalingTechnique == DownscalingTechnique::Software);
        fSession.fImageState.RefreshScreenSpace();
        UpdateMainRenderImages();
        fRefreshOperation.Queue();
    }

    OIV::FilterType Viewer::GetFilterType() const
    {
        return fSession.fImageFilter;
    }

    void Viewer::UpdateExposure()
    {
        fRenderer->SetExposure(fSession.fColorExposure);
        fRefreshOperation.Queue();
    }

    bool Viewer::ToggleColorCorrection()
    {
        bool isDefault = true;
        if (fSession.fColorExposure == fSession.DefaultColorCorrection)
        {
            std::swap(fSession.fLastColorExposure, fSession.fColorExposure);
        }
        else
        {
            isDefault = false;
            std::swap(fSession.fLastColorExposure, fSession.fColorExposure);
            fSession.fColorExposure = fSession.DefaultColorCorrection;
        }
        UpdateExposure();

        return isDefault;
    }

    void Viewer::SetViewFit(ViewFit fit, bool center)
    {
        fSession.fFitLargerOnly = fit == ViewFit::FitLargerOnly;
        if (fit != ViewFit::Original)
            FitToClientAreaAndCenter();
        else
        {
            fSession.fIsLockFitToScreen = false;
            SetOriginalSize();
            if (center)
                Center();
        }
    }

    bool Viewer::ScrollToPercent(int percent)
    {
        if (percent < 0 || percent > 100 || !IsImageOpen())
            return false;
        auto offset       = GetOffset();
        const auto canvas = fWindow.GetCanvasPixelSize();
        const auto image  = GetImageSize(ImageSizeType::Visible);
        offset.y          = image.y > canvas.y ? (canvas.y - image.y) * percent / 100.0 : (canvas.y - image.y) / 2.0;
        SetOffset(offset, true);
        return true;
    }

    void Viewer::ToggleFullScreen(bool multiFullScreen)
    {
        fRefreshOperation.Begin();
        const LWS::WindowMode currentMode   = fWindow.GetWindowMode();
        const LWS::WindowMode requestedMode = currentMode == LWS::WindowMode::Windowed
                                                  ? (multiFullScreen ? LWS::WindowMode::FullscreenAllMonitors
                                                                     : LWS::WindowMode::Fullscreen)
                                                  : LWS::WindowMode::Windowed;
        if (fWindow.SetWindowMode(requestedMode) == LWS::Result::Success)
            Center();
        fRefreshOperation.End();
    }

    void Viewer::ToggleBorders()
    {
        fShowBorders = !fShowBorders;
        {
            std::ignore = fWindow.GetWindow().SetWindowStyles(fShowBorders
                                                                  ? LWS::WindowStyleFlags(WindowChromeStyles)
                                                                  : LWS::WindowStyleFlags(LWS::WindowStyle::NoStyle));
            fWindow.UpdateLayout();
        }
    }

    void Viewer::SetSlideShowEnabled(bool enabled)
    {
        if (fSession.fSlideshowPolicy.IsEnabled() != enabled)
        {
            fSession.fSlideshowPolicy.SetEnabled(enabled);
            fTimerSlideShow.SetInterval(fSession.fSlideshowPolicy.GetTimerIntervalMs());
        }
    }

    void Viewer::SetFilterLevel(OIV::FilterType filterType)
    {
        fSession.fImageFilter = std::clamp(filterType, OIV::FilterType::None, OIV::FilterType::Lanczos3);
        UpdateMainRenderImages();

        fRefreshOperation.Queue();
    }

    void Viewer::ToggleGrid()
    {
        fSession.fIsGridEnabled = !fSession.fIsGridEnabled;
        UpdateRenderViewParams();
    }

    void Viewer::UpdateRenderViewParams()
    {
        const auto colors = ViewerPresentationPolicy::CheckerboardColors(fSession.fTransparencyMode);
        fRenderViewParams.uTransparencyColor1 = colors[0];
        fRenderViewParams.uTransparencyColor2 = colors[1];
        fRefreshOperation.Queue();
    }

    void Viewer::SetOffset(LLUtils::PointF64 offset, bool preserveOffsetLockState)
    {
        const auto previousOffset = fSession.fImageState.GetOffset();
        fSession.SetOffset(offset, preserveOffsetLockState);
        if (fSession.fImageState.IsDownscaled() && previousOffset != fSession.fImageState.GetOffset())
            QueueResampling();
        UpdateMainRenderImages();
        fPreserveImageSpaceSelection.Queue();
        fRefreshOperation.Queue();
    }

    void Viewer::SetOriginalSize()
    {
        SetZoomInternal(1.0, -1, -1);
        Center();
    }

    void Viewer::Pan(const LLUtils::PointF64& panAmount)
    {
        if (fSession.fImageState.GetOpenedImage() != nullptr)
            SetOffset(panAmount * fDPIadjustmentFactor + fSession.fImageState.GetOffset());
    }

    void Viewer::Zoom(double wheelSteps, int zoomX, int zoomY)
    {
        if (IsImageOpen())
        {
            CommandManager::CommandRequest request;
            request.displayName = "Zoom";
            request.args        = CommandManager::CommandArgs::FromString(
                "val=" + std::to_string(wheelSteps) + ";cx=" + std::to_string(zoomX) + ";cy=" + std::to_string(zoomY));
            request.commandName = "cmd_zoom";
            ExecuteCommand(request);
        }
    }

    void Viewer::ZoomInternal(double steps, int zoomX, int zoomY)
    {
        const double adaptiveAmount = fAdaptiveZoom.Add(steps);
        const double adjustedAmount = ViewActionController::RelativeZoom(GetScale(), adaptiveAmount);
        SetZoomInternal(adjustedAmount, zoomX, zoomY);
    }

    void Viewer::FitToClientAreaAndCenter()
    {
        if (IsImageOpen())
        {
            using namespace LLUtils;
            const LWS::PixelSize clientSize = fWindow.GetCanvasPixelSize();
            if (clientSize.x > 0 && clientSize.y > 0)  // window might minimized.
            {
                double zoom = fSession.FitScale();
                fRefreshOperation.Begin();
                fSession.fIsLockFitToScreen = true;
                SetZoomInternal(zoom, -1, -1, true);
                Center();
                fRefreshOperation.End();
            }
        }
    }

    void Viewer::UpdateSelectionRectText()
    {
        TextLabel* selectionSizeText = fLabelManager.GetTextLabel("selectionSizeText");
        auto selectionSizeStr        = SelectionWorkflowPolicy::FormatSelectionSize(fSession.fImageSpaceSelection);

        if (selectionSizeText == nullptr)
        {
            // Create new user message.
            selectionSizeText = fLabelManager.GetOrCreateTextLabel("selectionSizeText");
            selectionSizeText->SetFontSize(11);
            selectionSizeText->SetBackgroundColor({0, 0, 0, 192});
            selectionSizeText->SetTextColor({170, 170, 170, 255});
            selectionSizeText->SetTextRenderMode(FreeType::RenderMode::Antialiased);
            selectionSizeText->SetOutlineWidth(0);

            selectionSizeText->SetFilterType(OIV::FilterType::None);
            selectionSizeText->SetImageRenderMode(OIV::ImageRenderMode::Overlay);
            selectionSizeText->SetScale({1.0, 1.0});
            selectionSizeText->SetOpacity(1.0);
        }

        // text already exists, just make visible.
        selectionSizeText->SetVisible(true);
        selectionSizeText->SetText(selectionSizeStr);
        selectionSizeText->GetMetrics();

        const auto labelPosition = SelectionWorkflowPolicy::PlaceSelectionLabel(
            fSelectionRect.GetSelectionRect(),
            {static_cast<int32_t>(selectionSizeText->GetMetrics().pixelSize.x),
             static_cast<int32_t>(selectionSizeText->GetMetrics().pixelSize.y)},
            static_cast<LLUtils::PointI32>(fWindow.GetCanvasPixelSize()));

        selectionSizeText->SetPosition(static_cast<LLUtils::PointF64>(labelPosition));
    }

    void Viewer::SetImageSpaceSelection(const LLUtils::RectI32& rect)
    {
        fSession.SetSelection(rect);
        UpdateSelectionRectText();
    }

    void Viewer::SaveImageSpaceSelection()
    {
        if (SelectionWorkflowPolicy::ShouldSaveImageSpaceSelection(fSelectionRect.GetOperation()))
            SetImageSpaceSelection(ClientToImageRounded(fSelectionRect.GetSelectionRect()));
    }

    void Viewer::LoadImageSpaceSelection()
    {
        if (fSession.fImageSpaceSelection.IsEmpty() == false)
        {
            LLUtils::RectI32 r = static_cast<LLUtils::RectI32>(
                ImageToClient(static_cast<LLUtils::RectF64>(fSession.fImageSpaceSelection)));
            fSelectionRect.UpdateSelection(r);
            UpdateSelectionRectText();
        }
    }

    void Viewer::UpdateSelectionCursor(bool altPressed)
    {
        if (fAutoScroll->IsAutoScrolling())
            return;
        if (altPressed && (fSelectionRect.GetOperation() == SelectionRect::Operation::BeginDrag ||
                           fSelectionRect.GetOperation() == SelectionRect::Operation::Drag))
            return;
        auto shape = MainWindow::CursorType::SystemDefault;
        if (altPressed && fSelectionRect.GetOperation() == SelectionRect::Operation::EndDrag &&
            fWindow.GetCanvasWindow().IsMouseInClientRect())
        {
            switch (fSelectionRect.HitTest(fWindow.GetCanvasMousePosition()))
            {
                case SelectionRect::Handle::TopLeft:
                case SelectionRect::Handle::BottomRight:
                    shape = MainWindow::CursorType::ResizeNWSE;
                    break;
                case SelectionRect::Handle::TopRight:
                case SelectionRect::Handle::BottomLeft:
                    shape = MainWindow::CursorType::ResizeNESW;
                    break;
                case SelectionRect::Handle::Top:
                case SelectionRect::Handle::Bottom:
                    shape = MainWindow::CursorType::ResizeNS;
                    break;
                case SelectionRect::Handle::Left:
                case SelectionRect::Handle::Right:
                    shape = MainWindow::CursorType::ResizeEW;
                    break;
                case SelectionRect::Handle::Move:
                    shape = MainWindow::CursorType::SelectionMove;
                    break;
                default:
                    break;
            }
        }
        fWindow.SetCursorType(shape);
    }

    void Viewer::CancelSelection()
    {
        TextLabel* selectionSizeText = fLabelManager.GetTextLabel("selectionSizeText");
        if (selectionSizeText != nullptr)
            selectionSizeText->SetVisible(false);

        fSelectionRect.SetSelection(SelectionRect::Operation::CancelSelection, {0, 0});
        fSession.fImageSpaceSelection = decltype(fSession.fImageSpaceSelection)::Zero;
        UpdateSelectionCursor(false);
    }

    double Viewer::GetMinimumPixelSize()
    {
        return ViewActionController::MinimumPixelSize(fSession.fMinImageSize, GetImageSize(ImageSizeType::Transformed));
    }

    void Viewer::QueueResampling()
    {
        if (GetResamplingEnabled() && IsImageOpen() &&
            (fSession.fDownScalingTechnique == DownscalingTechnique::Software || fSession.fImageState.IsDownscaled()))
        {
            // Restart idle refinement while retaining usable cached detail.
            fTimerNoActiveZoom.SetInterval(0);
            fTimerNoActiveZoom.SetInterval(fSession.fImageState.IsDownscaled() ? fScreenSpaceDelayMs
                                                                               : fQueueResamplingDelay);
        }
    }

    void Viewer::SetResamplingEnabled(bool enable)
    {
        if (fIsResamplingEnabled != enable)
        {
            fIsResamplingEnabled = enable;
            if (fIsResamplingEnabled == false)
            {
                fTimerNoActiveZoom.SetInterval(0);
                fSession.fImageState.SetResample(false);
                UpdateMainRenderImages();
            }
            else
            {
                QueueResampling();
            }
        }
    }

    bool Viewer::GetResamplingEnabled() const
    {
        return fIsResamplingEnabled;
    }

    void Viewer::SetZoomInternal(double zoom, int clientX, int clientY, bool preserveFit)
    {
        if (fSession.ZoomTo(zoom, {clientX, clientY}, GetCanvasCenter(), preserveFit))
        {
            // State changes are complete before touching native resources. Keep selection restoration and
            // presentation coalesced so zoom never exposes a frame with the previous client-space selection.
            fPreserveImageSpaceSelection.Begin();
            fRefreshOperation.Begin();
            QueueResampling();
            UpdateMainRenderImages();
            fPreserveImageSpaceSelection.Queue();
            fRefreshOperation.Queue();
            fPreserveImageSpaceSelection.End();
            fRefreshOperation.End();
        }
    }

    double Viewer::GetScale() const
    {
        return fSession.fImageState.GetScale().x;
    }

    /*void Viewer::UpdateCanvasSize()
    {
        if (fSession.fImageState.GetRetainedImage(ImageProcessingStage::GeometricTransform) != nullptr)
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

    void Viewer::UpdateTexelPos()
    {
        if (fVirtualStatusBar.GetVisible() == true)
        {
            if (fSession.fImageState.GetRetainedImage(ImageProcessingStage::GeometricTransform) != nullptr)
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
                    LLUtils::native_string_type message = StringUtility::ToNativeString(OIVHelper::ParseTexelValue(
                        fSession.fImageState.GetRetainedImage(ImageProcessingStage::GeometricTransform),
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

    void Viewer::AutoPlaceImage(bool forceCenter)
    {
        fRefreshOperation.Begin();
        if (ViewActionController::ShouldFitToScreenOnAutoPlace(fSession.fIsLockFitToScreen, fSession.fIsOffsetLocked))
            FitToClientAreaAndCenter();
        else if (ViewActionController::ShouldCenterOnAutoPlace(fSession.fIsLockFitToScreen, fSession.fIsOffsetLocked,
                                                               forceCenter))
            Center();
        fRefreshOperation.End();
    }

    void Viewer::UpdateWindowSize()
    {
        const auto clientArea     = fWindow.GetCanvasWindow().GetClientAreaMetrics();
        const auto scale          = clientArea.Scale();
        const LWS::PixelSize size = clientArea.pixels.value_or(LWS::PixelSize{});
        fSession.SetViewport({size.x, size.y});
        // Forward zero extents as well: minimized or unconfigured canvases must suspend presentation.
        if (fViewportSize != size)
        {
            fRenderViewParams.uViewportSize = {size.x, size.y};
            fViewportSize                   = size;
        }
        if (scale.has_value() && size.x > 0 && size.y > 0)
        {
            fDPIadjustmentFactor = {scale->x, scale->y};
            fSelectionRect.SetContentScale(fDPIadjustmentFactor);
            fLabelManager.SetContentScale(*scale);
            UpdateWelcomeMessageLayout();
            AutoPlaceImage();
            if (fSession.fImageState.IsDownscaled())
                QueueResampling();
            const LLUtils::PointI32 point{size.x, size.y};
            fVirtualStatusBar.ClientSizeChanged(point);

            fMessageManager->OnWindowSizeChange(size);
        }
    }

    void Viewer::Center()
    {
        if (IsImageOpen() == true)
        {
            fRefreshOperation.Begin();
            using namespace LLUtils;
            PointF64 offset = fSession.CenterOffset(GetCanvasCenter());
            // Lock offset when centering
            fSession.fIsOffsetLocked = true;
            SetOffset(offset, true);
            fRefreshOperation.End();
        }
    }

    void Viewer::TransformImage(IMUtil::AxisAlignedRotation relativeRotation, IMUtil::AxisAlignedFlip flip)
    {
        fRefreshOperation.Begin();
        SetResamplingEnabled(false);
        fSession.fImageState.Transform(relativeRotation, flip);
        fSession.fImageState.Refresh(ImageProcessingStage::GeometricTransform);
        AutoPlaceImage(true);
        RefreshImage();
        fRefreshOperation.End();
        SetResamplingEnabled(true);
    }

    void Viewer::SetDownScalingTechnique(DownscalingTechnique technique)
    {
        if (technique != fSession.fDownScalingTechnique)
        {
            fSession.fDownScalingTechnique = technique;
            switch (fSession.fDownScalingTechnique)
            {
                case DownscalingTechnique::None:
                    fSession.fImageState.SetResample(false);
                    UpdateMainRenderImages();
                    break;
                case DownscalingTechnique::HardwareMipmaps:
                    fSession.fImageState.SetResample(false);
                    UpdateMainRenderImages();
                    break;
                case DownscalingTechnique::Software:
                    fSession.fImageState.SetResample(true);
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
