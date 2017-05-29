
#include "RawInputMouseWindow.h"
#include "../Win32Window.h"
namespace OIV
{
    namespace Win32
    {
        RawInputMouseWindow::RawInputMouseWindow(Win32WIndow* win)
        {
            fWin = win;
        }

        void RawInputMouseWindow::SetButtonState(Button button, State state)
        {
            MouseState::SetButtonState(button, state);

            if (state == Down && fWin->IsUnderMouseCursor())
            {
               fCaptured[button] = true;
            }
            else if (state == Up)
                fCaptured[button] = false;
        }

        bool RawInputMouseWindow::IsCaptured(Button button) const
        {
            return fCaptured[button] == true;
        }
    }
}
