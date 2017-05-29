#pragma once

#include "RawInputMouse.h"

// RawInputMouseWindow extends MouseState in a way to allow mouse to be captured relative to a window.
namespace OIV
{
    namespace Win32
    {
        class Win32WIndow;
        class RawInputMouseWindow : public MouseState
        {
            Win32WIndow* fWin;
            bool  fCaptured[Max_Buttons] = { false };
        protected:
            void SetButtonState(Button button, State state) override;
        public:
            RawInputMouseWindow(Win32WIndow* win);
            bool IsCaptured(Button button) const;
        };
    }
}