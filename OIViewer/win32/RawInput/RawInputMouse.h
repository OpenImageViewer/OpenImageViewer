#pragma once
#include  <windows.h>
#include <vector>
#include <StopWatch.h>
#include "../Win32Helper.h"

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
        protected:
            static const int Max_Buttons = 3;
        
            public:
            enum class Button { Left,Right,Middle,Third, Forth, Fifth};
            enum class State { NotSet, Down, Up };
            enum class EventType { NotSet, Pressed, Clicked, DoublePressed,  Released };

            ///////////////////////
            // Button event            
            struct ButtonEvent
            {
                int64_t timeStamp;
                Button button;
                EventType eventType;
                int32_t x;
                int32_t y;
            };
            
            typedef std::vector<ButtonEvent> ListButtonEvent;
            ///////////////////////
            uint16_t dDoubleClickTime = 250;

         
        private:
            LLUtils::StopWatch fTimer = LLUtils::StopWatch(true);
            State fButtons[Max_Buttons] = {State::NotSet };
            LONG fDeltaX = 0;
            LONG fDeltaY = 0;
            LONG fwheel = 0;
            

            ListButtonEvent fButtonActions;
            ListButtonEvent fButtonActionsHistory;

        public:
            MouseState()
            {
                dDoubleClickTime = GetDoubleClickTime();
            }
            
            LONG GetX() const { return fDeltaX; }
            LONG GetY() const { return fDeltaY; }
            LONG GetWheel() const { return fwheel; }

            virtual void SetButtonState(Button button , State state)
            {
                fButtons[static_cast<size_t>(button)] = state;
            }

            void Update(const RAWMOUSE& mouse)
            {
                LLUtils::StopWatch::time_type_integer elpased = fTimer.GetElapsedTimeInteger(LLUtils::StopWatch::Milliseconds);
                fDeltaX += mouse.lLastX;
                fDeltaY += mouse.lLastY;
                POINT mousePos = Win32Helper::GetMouseCursorPosition();
                if (mouse.ulButtons & 0x0400)
                    fwheel += static_cast<SHORT>(mouse.usButtonData) / WHEEL_DELTA;
                for (int i = 0; i < Max_Buttons; i++)
                {

                    State state = fButtons[i];
                    Button button = static_cast<Button>(i);
                    if (mouse.ulButtons & (1ul << (i * 2)))
                    {
                        SetButtonState(button, State::Down);
                        fButtonActions.push_back({elpased, button, EventType::Pressed ,mousePos.x, mousePos.y });

                        if (IsClicked(button, dDoubleClickTime, mousePos, 10 * 10))
                        {
                            fButtonActions.push_back({ elpased, button, EventType::DoublePressed, mousePos.x , mousePos.y });
                            ClearHistory(button);
                        }
                        else
                        {
                            fButtonActionsHistory.push_back({ elpased, button, EventType::Pressed, mousePos.x, mousePos.y });
                        }
                        
                    }
                    if (mouse.ulButtons & (2ul << (i * 2)))
                    {
                        SetButtonState(button, State::Up);
                        fButtonActions.push_back({ elpased, button, EventType::Released, mousePos.x , mousePos.y });
                        fButtonActionsHistory.push_back({ elpased, button, EventType::Released, mousePos.x , mousePos.y });
                    }
                }
                ClearHistory();
            }


            void ClearHistory(Button button)
            {
                for (int i = fButtonActionsHistory.size() - 1; i >= 0; i--)
                    if (fButtonActionsHistory[i].button == button)
                        fButtonActionsHistory.erase(fButtonActionsHistory.begin() +i );
            }


            void ClearHistory()
            {
                LLUtils::StopWatch::time_type_integer elpased = fTimer.GetElapsedTimeInteger(LLUtils::StopWatch::Milliseconds);
                while (fButtonActionsHistory.empty() == false && elpased - fButtonActionsHistory[0].timeStamp > dDoubleClickTime)
                    fButtonActionsHistory.erase(fButtonActionsHistory.begin());

            }

            bool IsClicked(Button button, uint64_t time = 1 << 31, POINT mousePos = { 0,0 }, uint32_t radiusSquared = 1 << 31)
            {
                LLUtils::StopWatch::time_type_integer elpased = fTimer.GetElapsedTimeInteger(LLUtils::StopWatch::Milliseconds);
                int state = 0;
                LLUtils::PointI32 origin = { mousePos.x,mousePos.y };
                for (ButtonEvent evnt : fButtonActionsHistory)
                {
                    if (evnt.button == button)
                    {
                        
                        if (elpased - evnt.timeStamp < time)
                        {
                            if (evnt.eventType == EventType::Pressed)
                            {
                                if (origin.DistanceSquared({ evnt.x, evnt.y }) < radiusSquared)
                                state = 1;
                            }
                            else if (evnt.eventType == EventType::Released && state == 1)
                            {
                                if (origin.DistanceSquared({ evnt.x , evnt.y }) < radiusSquared)
                                    return true;
                            }
                        }
                    }
                }

                return false;
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
                return fButtons[static_cast<int>(button)];
            }
        };
    }
}
    
