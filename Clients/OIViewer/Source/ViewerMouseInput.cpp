#include "ViewerMouseInput.h"

#include "ViewerApplication.h"
#include "OIVImage/OIVFileImage.h"

#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>

namespace OIV
{
    ViewerMouseInput::ViewerMouseInput(ViewerApplication& owner) : fOwner(owner), fMultiClick(owner.fPlatform, 500, 2)
    {
        fMultiClick.OnMouseClickEvent.Add([this](const auto& event) { OnMultiClick(event); });
    }

    void ViewerMouseInput::SetButton(LWS::MouseButton button, bool pressed, bool mouseInside)
    {
        if (fController.UpdateButtonState(button, pressed, mouseInside))
        {
            fMultiClick.SetButtonState(button, pressed);
            ApplyDecision(fController.ResolveButton(button, pressed, mouseInside, GetButtonContext(button)));
        }
    }

    MouseGestureController::ButtonContext ViewerMouseInput::GetButtonContext(LWS::MouseButton button) const
    {
        MouseGestureController::ButtonContext context;
        if (button == LWS::MouseButton::Left)
        {
            context.altPressed     = fOwner.fPlatform.IsKeyPressed(LWS::KeyCode::Alt).value_or(false);
            context.controlPressed = fOwner.fPlatform.IsKeyPressed(LWS::KeyCode::Control).value_or(false);
            context.windowed       = fOwner.fWindow.GetWindow().GetWindowMode() == LWS::WindowMode::Windowed;
        }
        return context;
    }

    void ViewerMouseInput::Move(LWS::Point delta)
    {
        fMultiClick.SetMouseDelta(static_cast<int16_t>(delta.x), static_cast<int16_t>(delta.y));
        ApplyDecision(fController.Move(delta, fOwner.fContextMenu->IsVisible()));
    }

    void ViewerMouseInput::ApplyDecision(const MouseGestureController::Decision& decision)
    {
        if (decision.contextMenuTimer == MouseGestureController::TimerChange::Stop)
            fOwner.fContextMenuTimer.SetInterval(0);
        if (decision.navigationTimer == MouseGestureController::TimerChange::Stop)
            fOwner.fTimerNavigation.SetInterval(0);
        if (decision.pointerLock == MouseGestureController::PointerLockChange::Unlock)
            std::ignore = fOwner.fWindow.GetCanvasWindow().SetPointerLocked(false);
        if (decision.resetMultiClick)
            fMultiClick.Reset();
        if (decision.stopAutoScroll)
            StopAutoScroll();
        if (decision.cancelSelection)
            fOwner.CancelSelection();
        if (decision.pointerLock == MouseGestureController::PointerLockChange::Lock)
            std::ignore = fOwner.fWindow.GetCanvasWindow().SetPointerLocked(true);
        if (decision.contextMenuTimer == MouseGestureController::TimerChange::Start &&
            fOwner.fContextMenu->IsSupported())
            fOwner.fContextMenuTimer.SetInterval(ContextMenuDelayMs);
        if (decision.navigationTimer == MouseGestureController::TimerChange::Start)
            fOwner.fTimerNavigation.SetInterval(fOwner.fQuickBrowseDelay);

        switch (decision.action)
        {
            case MouseGestureController::Action::CancelInput:
                Cancel();
                break;
            case MouseGestureController::Action::ToggleAutoScroll:
                ToggleAutoScroll();
                break;
            case MouseGestureController::Action::BeginSelection:
                fOwner.fSelectionRect.SetSelection(SelectionRect::Operation::BeginDrag,
                                                   fOwner.SnapToScreenSpaceImagePixels(
                                                       fOwner.fWindow.GetCanvasMousePosition()));
                fOwner.SaveImageSpaceSelection();
                break;
            case MouseGestureController::Action::EndSelection:
                fOwner.fSelectionRect.SetSelection(SelectionRect::Operation::EndDrag,
                                                   fOwner.SnapToScreenSpaceImagePixels(
                                                       fOwner.fWindow.GetCanvasMousePosition()));
                fOwner.SaveImageSpaceSelection();
                break;
            case MouseGestureController::Action::UpdateSelection:
                fOwner.fSelectionRect.SetSelection(SelectionRect::Operation::Drag,
                                                   fOwner.SnapToScreenSpaceImagePixels(
                                                       fOwner.fWindow.GetCanvasMousePosition()));
                fOwner.SaveImageSpaceSelection();
                break;
            case MouseGestureController::Action::Pan:
                fOwner.Pan(static_cast<LLUtils::PointF64>(decision.delta));
                break;
            case MouseGestureController::Action::BeginWindowDrag:
                if (fOwner.fWindow.GetWindow().BeginWindowDrag(decision.windowDragOperation) == LWS::Result::Success)
                    Cancel();
                break;
            case MouseGestureController::Action::JumpFiles:
                fOwner.JumpFiles(decision.fileStep);
                break;
            case MouseGestureController::Action::QueueCancelSelection:
                std::ignore = fOwner.fPlatform.PostTask(
                    fOwner.MakeSafeCallback([this]() { fOwner.CancelSelection(); }));
                break;
            case MouseGestureController::Action::QueueToggleFullScreen:
                std::ignore = fOwner.fPlatform.PostTask(fOwner.MakeSafeCallback(
                    [this, multiFullScreen = decision.multiFullScreen] { fOwner.ToggleFullScreen(multiFullScreen); }));
                break;
            case MouseGestureController::Action::QueuePaste:
                std::ignore = fOwner.fPlatform.PostTask(
                    fOwner.MakeSafeCallback([this]() { fOwner.ExecutePredefinedCommand("PasteImageFromClipboard"); }));
                break;
            case MouseGestureController::Action::None:
                break;
        }
    }

    void ViewerMouseInput::ToggleAutoScroll()
    {
        fOwner.fAutoScroll->ToggleAutoScroll();
        if (fOwner.fAutoScroll->IsAutoScrolling())
        {
            const auto path = LLUtils::StringUtility::ToNativeString(LLUtils::PlatformUtility::GetExeFolder()) +
                              LLUTILS_TEXT("./Resources/Cursors/ArrowC.cur");
            auto anchor     = std::make_unique<OIVFileImage>(path);
            if (anchor->Load(&fOwner.fImageLoader, IMCodec::PluginTraverseMode::AnyPlugin) == RC_Success)
            {
                anchor->SetImageRenderMode(OIV_Image_Render_mode::IRM_Overlay);
                anchor->SetPosition(static_cast<LLUtils::PointF64>(
                    static_cast<LLUtils::PointI32>(fOwner.fWindow.GetCanvasMousePosition()) -
                    static_cast<LLUtils::PointI32>(anchor->GetImage()->GetDimensions()) / 2));
                anchor->SetScale({1.0, 1.0});
                anchor->SetOpacity(0.5);
                anchor->SetVisible(true);
                fOwner.fAutoScrollAnchor = std::move(anchor);
            }
        }
        else
        {
            fOwner.fWindow.SetCursorType(MainWindow::CursorType::SystemDefault);
            fOwner.fAutoScrollAnchor.reset();
        }
    }

    void ViewerMouseInput::StopAutoScroll()
    {
        if (fOwner.fAutoScroll->IsAutoScrolling())
            ToggleAutoScroll();
    }

    void ViewerMouseInput::Wheel(double steps, bool mouseInside)
    {
        if (steps != 0.0 && !fController.IsRockerActive())
        {
            const bool rightCaptured = fController.IsCaptured(LWS::MouseButton::Right);
            // Navigation intentionally reacts to every event by sign. High-resolution wheels can therefore trigger
            // multiple navigation commands while moving through one logical detent.
            if (mouseInside && fOwner.fPlatform.IsKeyPressed(LWS::KeyCode::Alt).value_or(false))
                fOwner.ExecutePredefinedCommand(steps > 0.0 ? "PreviousSubImage" : "NextSubImage");
            else if (mouseInside && fOwner.fPlatform.IsKeyPressed(LWS::KeyCode::Shift).value_or(false))
                fOwner.ExecutePredefinedCommand(steps > 0.0 ? "PreviousImageInFolder" : "NextImageInFolder");
            else if (rightCaptured)
                fOwner.Zoom(steps);
            else if (mouseInside)
            {
                const auto position = fOwner.fWindow.GetCanvasMousePosition();
                fOwner.Zoom(steps, position.x, position.y);
            }
        }
    }

    void ViewerMouseInput::OnMultiClick(const MouseMultiClickHandler::EventArgs& event)
    {
        const bool selectionActive = fOwner.fSelectionRect.GetOperation() != SelectionRect::Operation::NoOp;
        ApplyDecision(fController.OnMultiClick(
            event.button, event.clickCount,
            {.selectionActive = selectionActive,
             .multiFullScreen = fOwner.fPlatform.IsKeyPressed(LWS::KeyCode::Alt).value_or(false)}));
    }

    void ViewerMouseInput::Cancel()
    {
        std::ignore = fOwner.fWindow.GetCanvasWindow().SetPointerLocked(false);
        fController.Reset();
        fMultiClick.Reset();
        fOwner.fContextMenuTimer.SetInterval(0);
        fOwner.fTimerNavigation.SetInterval(0);
    }

    int ViewerMouseInput::GetNavigationDirection() const
    {
        return fController.GetNavigationDirection();
    }
}  // namespace OIV
