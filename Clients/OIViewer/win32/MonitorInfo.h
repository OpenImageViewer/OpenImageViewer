#pragma once
#include <windows.h>
#include <map>
#include <vector>
#include <LLUtils/Singleton.h>
namespace OIV
{

    struct MonitorDesc
    {
        DISPLAY_DEVICE DisplayInfo;
        DEVMODE DisplaySettings;
        MONITORINFOEX monitorInfo;
        HMONITOR handle;
        uint16_t DPIx;
        uint16_t DPIy;
    };

    class MonitorInfo  : public LLUtils::Singleton<MonitorInfo>
    {
    public:
        MonitorInfo();
        void Refresh();

        const MonitorDesc& getMonitorInfo(size_t monitorIndex, bool allowRefresh = false);
        const MonitorDesc& getMonitorInfo(HMONITOR hMonitor, bool allowRefresh = false);
        const size_t getMonitorsCount() const;
        RECT getBoundingMonitorArea();
    private:
        using OSVersionInfo = RTL_OSVERSIONINFOW;
        static OSVersionInfo GetOSVersion() ;
        using MapHMonitorToDesc = std::map<HMONITOR, MonitorDesc>;
        std::vector<MonitorDesc> mDisplayDevices;
        MapHMonitorToDesc mHMonitorToDesc;
        inline static MonitorDesc mEmptyMonitorDesc = { };

        static BOOL CALLBACK MonitorEnumProc(
            _In_  HMONITOR hMonitor,
            _In_  HDC hdcMonitor,
            _In_  LPRECT lprcMonitor,
            _In_  LPARAM dwData
        );
    };

}
