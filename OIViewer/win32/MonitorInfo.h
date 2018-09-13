#pragma once
#include <windows.h>
#include <map>
#include <vector>
#include <Singleton.h>
namespace OIV
{

    struct MonitorDesc
    {
        DISPLAY_DEVICE DisplayInfo;
        DEVMODE DisplaySettings;
        MONITORINFOEX monitorInfo;
        HMONITOR handle;
    };

    class MonitorInfo  : public Singleton<MonitorInfo>
    {
    public:
        MonitorInfo();
        void Refresh();

        const MonitorDesc * const getMonitorInfo(unsigned short monitorIndex, bool allowRefresh = false);
        const MonitorDesc * const getMonitorInfo(HMONITOR hMonitor, bool allowRefresh = false);
        const unsigned short getMonitorsCount() const;
        RECT getBoundingMonitorArea();
    private:
        using MapHMonitorToDesc = std::map<HMONITOR, MonitorDesc>;
        std::vector<MonitorDesc> mDisplayDevices;
        MapHMonitorToDesc mHMonitorToDesc;

        static BOOL CALLBACK MonitorEnumProc(
            _In_  HMONITOR hMonitor,
            _In_  HDC hdcMonitor,
            _In_  LPRECT lprcMonitor,
            _In_  LPARAM dwData
        );
    };

}
