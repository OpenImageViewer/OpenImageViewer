#pragma once
#include  <windows.h>
#include <vector>

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
            enum State { NotSet, Down, Up, Pressed, Released};
            
            struct ButtonEvent
            {
                Button button;
                //Pressed or released.
                State state;
            };
            static const int Max_Buttons = 3;
            
            typedef std::vector<ButtonEvent> ListButtonEvent;
        private:
            State fButtons[Max_Buttons] = { NotSet };
            LONG fDeltaX = 0;
            LONG fDeltaY = 0;
            LONG fwheel = 0;
            

            ListButtonEvent fButtonActions;

        public:

            LONG GetX() const { return fDeltaX; }
            LONG GetY() const { return fDeltaY; }
            LONG GetWheel() const { return fwheel; }

            virtual void SetButtonState(Button button , State state)
            {
                fButtons[button] = state;
            }

            void Update(const RAWMOUSE& mouse)
            {
                fDeltaX += mouse.lLastX;
                fDeltaY += mouse.lLastY;
                if (mouse.ulButtons & 0x0400)
                    fwheel += static_cast<SHORT>(mouse.usButtonData) / WHEEL_DELTA;
                for (int i = 0; i < Max_Buttons; i++)
                {

                    State state = fButtons[i];
                    Button button = static_cast<Button>(i);
                    if (mouse.ulButtons & (1ul << (i * 2)))
                    {
                        SetButtonState(button, Down);
                        fButtonActions.push_back({ button,Pressed });
                    }
                    if (mouse.ulButtons & (2ul << (i * 2)))
                    {
                        SetButtonState(button, Up);
                        fButtonActions.push_back({ button, Released });
                    }
                }
            }

            void Flush()
            {
                fDeltaX = fDeltaY = fwheel = 0;
                fButtonActions.clear();
            }

            
            ListButtonEvent&& MoveButtonActions()
            {
                return std::move(fButtonActions);
            }


            State GetButtonState(Button button) const
            {
                return fButtons[button];
            }
        };
    }
}
    
