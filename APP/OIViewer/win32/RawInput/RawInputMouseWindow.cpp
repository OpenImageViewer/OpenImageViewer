
#include "RawInputMouseWindow.h"
#include "../Win32Window.h"
namespace OIV
{
    namespace Win32
    {
        RawInputMouseWindow::RawInputMouseWindow(Win32Window* win)
        {
            fWin = win;
        }

        void RawInputMouseWindow::SetButtonState(Button button, State state)
        {
            MouseState::SetButtonState(button, state);

            if (state == State::Down && fWin->IsUnderMouseCursor())
            {
               fCaptured[static_cast<size_t>(button)] = true;
            }
            else if (state == State::Up)
                fCaptured[static_cast<size_t>(button)] = false;
        }

        bool RawInputMouseWindow::IsCaptured(Button button) const
        {
            return fCaptured[static_cast<size_t>(button)] == true;
        }
    }
}
