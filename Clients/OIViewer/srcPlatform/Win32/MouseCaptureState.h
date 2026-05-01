#pragma once

#include <LInput/Buttons/ButtonStates.h>
#include <LInput/Mouse/MouseButton.h>
#include <LInput/Win32/RawInput/RawInput.h>

#include <array>
#include <cstddef>

namespace OIV
{
    class MouseCaptureState
    {
      public:
        void Update(LInput::MouseButton button, LInput::ButtonState state, bool mouseUnderWindow)
        {
            const size_t index = static_cast<size_t>(button);
            if (state == LInput::ButtonState::Down && mouseUnderWindow)
                fCapturedButtons[index] = true;
            else if (state == LInput::ButtonState::Up)
                fCapturedButtons[index] = false;
        }

        bool IsCaptured(LInput::MouseButton button) const
        {
            return fCapturedButtons.at(static_cast<size_t>(button));
        }

      private:
        std::array<bool, LInput::RawInput::MaxMouseButtons> fCapturedButtons{};
    };
}  // namespace OIV
