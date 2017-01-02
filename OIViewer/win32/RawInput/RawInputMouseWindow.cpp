
#include "RawInputMouseWindow.h"
#include "../Win32Window.h"
namespace OIV
{
    namespace Win32
    {
        void RawInputMouseWindow::SetButtonState(Button button, State state)
        {
            MouseState::SetButtonState(button, state);

            if (state == Down && fWin->IsInFocus() && fWin->IsMouseCursorInClientRect())
            {
               fCaptured[button] = true;
            }
            else if (state == Up)
                fCaptured[button] = false;
        }
    }
}
