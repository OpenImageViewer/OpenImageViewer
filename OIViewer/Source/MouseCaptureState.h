#pragma once

#include <LWS/MouseButton.hpp>

#include <array>
#include <cstddef>

namespace OIV
{
    class MouseCaptureState
    {
      public:

        void Update(LWS::MouseButton button, bool pressed, bool mouseUnderWindow)
        {
            const size_t index = static_cast<size_t>(button);
            if (pressed && mouseUnderWindow)
                fCapturedButtons[index] = true;
            else if (!pressed)
                fCapturedButtons[index] = false;
        }

        bool IsCaptured(LWS::MouseButton button) const { return fCapturedButtons.at(static_cast<size_t>(button)); }
        void Reset() { fCapturedButtons.fill(false); }

      private:

        std::array<bool, static_cast<size_t>(LWS::MouseButton::Count)> fCapturedButtons{};
    };
}  // namespace OIV
