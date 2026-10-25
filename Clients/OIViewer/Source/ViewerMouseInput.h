#pragma once

#include "MouseGestureController.h"
#include "MouseMultiClickHandler.h"

#include <LWS/MouseButton.hpp>
#include <LWS/WindowTypes.hpp>

#include <cstdint>

namespace OIV
{
    class Viewer;

    class ViewerMouseInput final
    {
      public:

        explicit ViewerMouseInput(Viewer& owner);

        void SetButton(LWS::MouseButton button, bool pressed, bool mouseInside);
        void Move(LWS::Point delta);
        // A value of 1.0 is one logical wheel detent (120 platform delta units).
        // mouseInside means the cursor is over the exposed canvas, not just within its bounds.
        void Wheel(double steps, bool mouseInside);
        void Cancel();
        void CaptureLost();
        [[nodiscard]] int GetNavigationDirection() const;

      private:

        static constexpr uint32_t ContextMenuDelayMs = 500;

        void ApplyDecision(const MouseGestureController::Decision& decision);
        [[nodiscard]] MouseGestureController::ButtonContext GetButtonContext(LWS::MouseButton button) const;
        void OnMultiClick(const MouseMultiClickHandler::EventArgs& event);
        void ToggleAutoScroll();
        void StopAutoScroll();

        Viewer& fOwner;
        MouseGestureController fController;
        MouseMultiClickHandler fMultiClick;
        decltype(MouseMultiClickHandler::OnMouseClickEvent)::Subscription fMultiClickSubscription;
    };
}  // namespace OIV
