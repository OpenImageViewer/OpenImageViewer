
#include "RawInputMouseWindow.h"
#include "../Win32Window.h"
namespace OIV
{
    namespace Win32
    {
        void RawInputMouseWindow::SetButtonState(Button button, State state)
        {
            MouseState::SetButtonState(button, state);

            if (state == Pressed)
            {
                RECT rect;
                fWin->GetClientRectangle(rect);

                if (PtInRect(&rect, fWin->GetMousePosition()) == TRUE)
                {
                    fCaptured[button] = true;
                }
            }
            else if (state == Released)
                fCaptured[button] = false;
        }
    }
}
