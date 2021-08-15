#pragma once
#include "Win32/MonitorInfo.h"

namespace OIV
{
    class MonitorProvider
    {
    public:
        void UpdateFromWindowHandle(HWND hwnd)
        {
            HMONITOR hmonitor = MonitorFromWindow(hwnd, 0);
            if (hmonitor != fMonitorDesc.handle) // update frame rate only if monitor has changed.
            {
                ::Win32::MonitorInfo::GetSingleton().Refresh(); // refresh in case monitors were added or removed since last refresh.
                fMonitorDesc = ::Win32::MonitorInfo::GetSingleton().getMonitorInfo(hmonitor);
                //fMonitorProvider.SetCurrentMonitor(MonitorInfo::GetSingleton().getMonitorInfo(hmonitor));

                EventManager::MonitorChangeEventParams params = { fMonitorDesc };
                EventManager::GetSingleton().MonitorChange.Raise(params);
            }
        }

    private:
        ::Win32::MonitorDesc fMonitorDesc{};
    };
}
