#pragma once
#include <LLUtils/Event.h>
#include <LLUtils/Singleton.h>
#include "win32/MonitorInfo.h"

namespace OIV
{
    class EventManager : public LLUtils::Singleton<EventManager>
    {
    public:

        struct MonitorChangeEventParams
        {
            MonitorDesc monitorDesc;
        };
        using MonitorChangeEvent = LLUtils::Event<void(const MonitorChangeEventParams&)>;
        
        MonitorChangeEvent MonitorChange;

        
    };
}
