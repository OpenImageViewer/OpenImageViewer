#pragma once

#include "RawInputMouse.h"

// RawInputMouseWindow extends MouseState in a way to allow mouse to be captured relative to a window.
namespace OIV
{
    namespace Win32
    {
        class Win32Window;
        class RawInputMouseWindow : public MouseState
        {
            Win32Window* fWin;
            bool  fCaptured[Max_Buttons] = { false };
        protected:
            void SetButtonState(Button button, State state) override;
        public:
            RawInputMouseWindow(Win32Window* win);
            bool IsCaptured(Button button) const;
        };
    }
}