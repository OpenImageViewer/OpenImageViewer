#pragma once

#include "MouseCaptureState.h"

#include <LWS/MouseButton.hpp>
#include <LWS/WindowTypes.hpp>

#include <LLUtils/Point.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace OIV
{
    // OIViewer intentionally models one logical pointer. LWS exposes window-system mouse events without a stable
    // cross-platform pointer identity, and operations such as pointer lock, native window drag, context menus, and
    // selection have one application-wide owner. Multiple physical mice therefore contribute to the same logical
    // pointer instead of receiving independent gesture state.
    //
    // A device map alone would not provide multi-mouse support. Supporting it requires stable pointer identities and
    // positions in the Win32 and Wayland LWS events, per-pointer capture, multi-click, gesture, drag, and cancellation
    // state, plus arbitration for global window operations. Independent visible pointers additionally require
    // rendered cursors and per-pointer hit testing.
    class MouseGestureController final
    {
      public:

        enum class Action
        {
            None,
            CancelInput,
            ToggleAutoScroll,
            BeginSelection,
            EndSelection,
            UpdateSelection,
            Pan,
            BeginWindowDrag,
            JumpFiles,
            QueueCancelSelection,
            QueueToggleFullScreen,
            QueuePaste,
        };

        enum class PointerLockChange
        {
            None,
            Lock,
            Unlock,
        };

        enum class TimerChange
        {
            None,
            Start,
            Stop,
        };

        struct ButtonContext
        {
            bool altPressed{};
            bool controlPressed{};
            bool windowed{};
        };

        struct MultiClickContext
        {
            bool selectionActive{};
            bool multiFullScreen{};
        };

        struct Decision
        {
            Action action                 = Action::None;
            PointerLockChange pointerLock = PointerLockChange::None;
            TimerChange contextMenuTimer  = TimerChange::None;
            TimerChange navigationTimer   = TimerChange::None;
            LWS::Point delta{};
            LWS::WindowDragOperation windowDragOperation = LWS::WindowDragOperation::Move;
            int fileStep{};
            bool cancelSelection{};
            bool resetMultiClick{};
            bool stopAutoScroll{};
            bool multiFullScreen{};
        };

        // Button state is updated before multi-click recognition. ResolveButton is called afterwards so a press that
        // completes a multi-click can suppress its otherwise competing single-button gesture.
        [[nodiscard]] bool UpdateButtonState(LWS::MouseButton button, bool pressed, bool mouseInside);
        [[nodiscard]] Decision ResolveButton(LWS::MouseButton button, bool pressed, bool mouseInside,
                                             const ButtonContext& context);
        [[nodiscard]] Decision Move(LWS::Point delta, bool contextMenuVisible);
        [[nodiscard]] Decision OnMultiClick(LWS::MouseButton button, uint16_t clickCount,
                                            const MultiClickContext& context);

        void Reset();
        [[nodiscard]] bool HasPressedButtons() const;
        [[nodiscard]] bool IsCaptured(LWS::MouseButton button) const;
        [[nodiscard]] bool IsRockerActive() const;
        [[nodiscard]] int GetNavigationDirection() const;

      private:

        static constexpr size_t ButtonCount           = static_cast<size_t>(LWS::MouseButton::Count);
        static constexpr int64_t DragThresholdSquared = 25;
        using ButtonState                             = std::array<bool, ButtonCount>;

        enum class Gesture
        {
            None,
            PendingWindowDrag,
            Selection,
            Pan,
            Rocker,
        };

        [[nodiscard]] Decision ActivateRocker(int fileStep);
        [[nodiscard]] bool AccumulateDragDelta(LWS::Point delta);
        [[nodiscard]] bool IsDown(LWS::MouseButton button) const;

        ButtonState fButtons{};
        MouseCaptureState fCapture;
        Gesture fGesture                              = Gesture::None;
        LWS::WindowDragOperation fWindowDragOperation = LWS::WindowDragOperation::Move;
        LLUtils::Point<int64_t> fDragDelta{};
        bool fConsumeButtonGesture{};
    };
}  // namespace OIV
