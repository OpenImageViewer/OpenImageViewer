#pragma once
#include <LLUtils/Event.h>
#include <LLUtils/Singleton.h>
#include <Win32/MonitorInfo.h>

namespace OIV
{
    class EventManager : public LLUtils::Singleton<EventManager>
    {
    public:

        struct MonitorChangeEventParams
        {
            ::Win32::MonitorDesc monitorDesc;
        };

        using MonitorChangeEvent = LLUtils::Event<void(const MonitorChangeEventParams&)>;
        
        MonitorChangeEvent MonitorChange;


        struct SizeChangeEventParams
        {
            int32_t width;
            int32_t height;
            
        };

        using SizeChangeEvent = LLUtils::Event<void(const SizeChangeEventParams&)>;

        SizeChangeEvent SizeChange;

        
    };
}
