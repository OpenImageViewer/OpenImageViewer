#include "MouseGestureController.h"
#include <algorithm>

namespace OIV
{
    bool MouseGestureController::UpdateButtonState(LWS::MouseButton button, bool pressed, bool mouseInside)
    {
        const size_t index = static_cast<size_t>(button);
        const bool changed = fButtons[index] != pressed;
        if (changed)
        {
            fButtons[index] = pressed;
            fCapture.Update(button, pressed, pressed && mouseInside);
        }
        return changed;
    }

    MouseGestureController::Decision MouseGestureController::ResolveButton(LWS::MouseButton button, bool pressed,
                                                                           bool mouseInside,
                                                                           const ButtonContext& context)
    {
        Decision decision;
        if (fConsumeButtonGesture)
        {
            fConsumeButtonGesture = false;
            decision.action       = Action::CancelInput;
        }
        else
        {
            const bool leftDown      = IsDown(LWS::MouseButton::Left);
            const bool rightDown     = IsDown(LWS::MouseButton::Right);
            const bool rightCaptured = fCapture.IsCaptured(LWS::MouseButton::Right);

            if (button == LWS::MouseButton::Middle)
            {
                if (pressed && mouseInside && fGesture == Gesture::None)
                    decision.action = Action::ToggleAutoScroll;
            }
            else if (button == LWS::MouseButton::Left)
            {
                if (pressed && rightDown && mouseInside)
                {
                    decision = ActivateRocker(-1);
                }
                else if (pressed && mouseInside && fGesture != Gesture::Rocker)
                {
                    decision.stopAutoScroll = true;
                    fDragDelta              = {};
                    if (context.altPressed)
                    {
                        fGesture        = Gesture::Selection;
                        decision.action = Action::BeginSelection;
                    }
                    else if (!rightCaptured && context.windowed)
                    {
                        fGesture             = Gesture::PendingWindowDrag;
                        fWindowDragOperation = context.controlPressed ? LWS::WindowDragOperation::ResizeNearest
                                                                      : LWS::WindowDragOperation::Move;
                    }
                }
                else if (!pressed)
                {
                    if (fGesture == Gesture::Selection)
                        decision.action = Action::EndSelection;
                    if (fGesture == Gesture::Selection || fGesture == Gesture::PendingWindowDrag)
                    {
                        fGesture   = Gesture::None;
                        fDragDelta = {};
                    }
                }
            }
            else if (button == LWS::MouseButton::X1 || button == LWS::MouseButton::X2)
            {
                if (pressed && mouseInside && fGesture == Gesture::None)
                    decision.navigationTimer = TimerChange::Start;
                else if (!fCapture.IsCaptured(LWS::MouseButton::X1) && !fCapture.IsCaptured(LWS::MouseButton::X2))
                    decision.navigationTimer = TimerChange::Stop;
            }
            else if (button == LWS::MouseButton::Right)
            {
                if (pressed && mouseInside)
                {
                    if (leftDown)
                    {
                        decision = ActivateRocker(1);
                    }
                    else if (fGesture != Gesture::Rocker)
                    {
                        fGesture                  = Gesture::Pan;
                        fDragDelta                = {};
                        decision.stopAutoScroll   = true;
                        decision.pointerLock      = PointerLockChange::Lock;
                        decision.contextMenuTimer = TimerChange::Start;
                    }
                }
                else if (!pressed)
                {
                    decision.contextMenuTimer = TimerChange::Stop;
                    if (fGesture == Gesture::Pan)
                    {
                        fGesture             = Gesture::None;
                        fDragDelta           = {};
                        decision.pointerLock = PointerLockChange::Unlock;
                    }
                }
            }

            if (!leftDown && !rightDown && fGesture == Gesture::Rocker)
                fGesture = Gesture::None;
        }
        return decision;
    }

    MouseGestureController::Decision MouseGestureController::Move(LWS::Point delta, bool contextMenuVisible)
    {
        Decision decision;
        const bool leftDown  = IsDown(LWS::MouseButton::Left);
        const bool rightDown = IsDown(LWS::MouseButton::Right);
        if (fGesture == Gesture::Selection && leftDown)
        {
            decision.action = Action::UpdateSelection;
        }
        else if (fGesture == Gesture::Pan && rightDown && !contextMenuVisible && delta != LWS::Point{})
        {
            decision.action = Action::Pan;
            decision.delta  = delta;
            if (AccumulateDragDelta(delta))
                decision.contextMenuTimer = TimerChange::Stop;
        }
        else if (fGesture == Gesture::PendingWindowDrag && leftDown && AccumulateDragDelta(delta))
        {
            decision.action              = Action::BeginWindowDrag;
            decision.windowDragOperation = fWindowDragOperation;
            decision.resetMultiClick     = true;
            fGesture                     = Gesture::None;
        }
        return decision;
    }

    MouseGestureController::Decision MouseGestureController::OnMultiClick(LWS::MouseButton button, uint16_t clickCount,
                                                                          const MultiClickContext& context)
    {
        Decision decision;
        if (clickCount == 2 && fCapture.IsCaptured(button))
        {
            const bool leftDown  = IsDown(LWS::MouseButton::Left);
            const bool rightDown = IsDown(LWS::MouseButton::Right);
            if (button == LWS::MouseButton::Left && !rightDown)
            {
                decision.action          = context.selectionActive ? Action::QueueCancelSelection
                                                                   : Action::QueueToggleFullScreen;
                decision.multiFullScreen = context.multiFullScreen;
                fConsumeButtonGesture    = true;
            }
            else if (button == LWS::MouseButton::Right && !leftDown)
            {
                decision.action       = Action::QueuePaste;
                fConsumeButtonGesture = true;
            }
        }
        return decision;
    }

    void MouseGestureController::Reset()
    {
        fButtons.fill(false);
        fCapture.Reset();
        fGesture              = Gesture::None;
        fWindowDragOperation  = LWS::WindowDragOperation::Move;
        fDragDelta            = {};
        fConsumeButtonGesture = false;
    }

    bool MouseGestureController::IsCaptured(LWS::MouseButton button) const
    {
        return fCapture.IsCaptured(button);
    }

    bool MouseGestureController::IsRockerActive() const
    {
        return fGesture == Gesture::Rocker;
    }

    int MouseGestureController::GetNavigationDirection() const
    {
        int direction = 0;
        if (IsDown(LWS::MouseButton::X2))
            direction = 1;
        else if (IsDown(LWS::MouseButton::X1))
            direction = -1;
        return direction;
    }

    MouseGestureController::Decision MouseGestureController::ActivateRocker(int fileStep)
    {
        Decision decision;
        decision.action           = Action::JumpFiles;
        decision.pointerLock      = PointerLockChange::Unlock;
        decision.contextMenuTimer = TimerChange::Stop;
        decision.navigationTimer  = TimerChange::Stop;
        decision.fileStep         = fileStep;
        decision.cancelSelection  = fGesture == Gesture::Selection;
        decision.resetMultiClick  = true;
        decision.stopAutoScroll   = true;
        fCapture.Reset();
        fGesture   = Gesture::Rocker;
        fDragDelta = {};
        return decision;
    }

    bool MouseGestureController::AccumulateDragDelta(LWS::Point delta)
    {
        // Native window drag remains pending through normal click jitter, preserving multi-click recognition until
        // movement makes the user's drag intent unambiguous.
        fDragDelta += static_cast<LLUtils::Point<int64_t>>(delta);
        return fDragDelta.x * fDragDelta.x + fDragDelta.y * fDragDelta.y > DragThresholdSquared;
    }

    bool MouseGestureController::HasPressedButtons() const
    {
        return std::ranges::any_of(fButtons, [](bool pressed) { return pressed; });
    }

    bool MouseGestureController::IsDown(LWS::MouseButton button) const
    {
        return fButtons[static_cast<size_t>(button)];
    }
}  // namespace OIV
