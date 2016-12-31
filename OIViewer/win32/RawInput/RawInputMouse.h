#pragma once
#include  <windows.h>
#include <stdexcept>
#include "Logger.h"
#include <sstream>

namespace OIV
{
    namespace Win32
    {
        class RawInput
        {
        public:

            static void ResiterWindow(HWND hWnd)
            {
                RAWINPUTDEVICE Rid[1];
                Rid[0].usUsagePage = 0x01;
                Rid[0].usUsage = 0x02; // 0x6 keyboard
                Rid[0].dwFlags = RIDEV_INPUTSINK;
                Rid[0].hwndTarget = hWnd;
                if (RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])) == false)
                {
                    throw std::runtime_error("Could not register raw input");
                }
            }
        };

        


        class MouseState
        {
        public:
            enum Button { Left,Right,Middle,Third, Forth, Fifth};
            enum State { NotSet, Down, Up, Pressed, Released };
            static const int Max_Buttons = 3;
        private:
            State fButtons[Max_Buttons] = { NotSet };
            LONG fDeltaX = 0;
            LONG fDeltaY = 0;
            LONG fwheel = 0;
        public:

            LONG GetX() const { return fDeltaX; }
            LONG GetY() const { return fDeltaY; }
            LONG GetWheel() const { return fwheel; }
            
            bool IsButtonPressed(Button button) const
            {
                return fButtons[button] == Pressed;
            }

            bool IsButtonDown(Button button) const
            {
                return fButtons[button] == Down;
            }
            virtual void SetButtonState(Button button , State state)
            {
                fButtons[button] = state;
            }

            void Update(const RAWMOUSE& mouse)
            {
                fDeltaX = mouse.lLastX;
                fDeltaY = mouse.lLastY;
                if (mouse.ulButtons & 0x0400)
                    fwheel = static_cast<SHORT>(mouse.usButtonData) / WHEEL_DELTA;
                else
                    fwheel = 0;

                for (int i = 0; i < Max_Buttons; i++)
                {

                    State state = fButtons[i];
                    Button button = static_cast<Button>(i);
                    if (mouse.ulButtons & (1ul << (i * 2)))
                        SetButtonState(button, Pressed);
                    else if (state == Pressed)
                        SetButtonState(button, Down);
                    
                    if (mouse.ulButtons & (2ul << (i * 2)))
                        SetButtonState(button, Released);
                    else if (state == Released)
                        SetButtonState(button, Up);
                }
            }

            State GetButtonState(Button button) const
            {
                return fButtons[button];
            }
        };
    }
}
    
