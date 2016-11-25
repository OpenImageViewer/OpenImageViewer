#pragma once
#include <functional>
#include <vector>
namespace OIV
{
    namespace Win32
    {
        class Win32WIndow;
        class Event
        {
        public:
            Win32WIndow* window;
            Event()
            {
                window = nullptr;
            }
            virtual ~Event()
            {
                
            }
        };

        class EventWinMessage : public Event
        {
        public:
            EventWinMessage()
            {
                memset(&message, 0, sizeof(MSG));
            }
            MSG message;
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
