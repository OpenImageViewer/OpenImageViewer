#pragma once
#include <functional>
#include <vector>
#include "RawInput/RawInputMouse.h"
namespace OIV
{
    namespace Win32
    {
        class Win32WIndow;
        class Event
        {
        public:
            Win32WIndow* window = nullptr;
            virtual ~Event() {}
        };

        class EventWinMessage : public Event
        {
        public:
            MSG message = { 0 };
        };

        class EventRawInputMouseStateChanged : public Event
        {
        public:
            int16_t DeltaX;
            int16_t DeltaY;
            int16_t DeltaWheel;
            MouseState::ListButtonEvent ChangedButtons;

            MouseState::EventType GetButtonEvent(MouseState::Button button) const
            {
                MouseState::EventType result = MouseState::EventType::NotSet;
                for (auto s : ChangedButtons)
                {
                    if (s.button == button)
                        result = s.eventType;
                }

                return result;
            }

        };

        typedef std::function< bool(const Event*) > EventCallback;
        typedef std::vector <EventCallback> EventCallbackCollection;
        

        class EventDragDrop : public Event
        {
            
        };


        class EventDdragDropFile : public EventDragDrop
        {
        public:
            std::wstring fileName;
        };
    }
}
