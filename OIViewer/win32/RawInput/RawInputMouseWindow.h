#pragma once

#include "RawInputMouse.h"

namespace OIV
{

    namespace Win32
    {
        class Win32WIndow;
        class RawInputMouseWindow : public MouseState
        {
            Win32WIndow* fWin;
            bool  fCaptured[Max_Buttons] = { false };
        public:
            RawInputMouseWindow(Win32WIndow* win)
            {
                fWin = win;
            }

            void SetButtonState(Button button, State state) override;
            bool IsCaptured(Button button) const { return fCaptured[button] == true; }


        };

      
    }
}


