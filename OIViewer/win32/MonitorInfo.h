#pragma once
#include <windows.h>
#include <map>
#include <vector>
namespace OIV
{
    class MonitorInfo 
    {
    private:
        static MonitorInfo sInstance;
        MonitorInfo();
    public:
        static MonitorInfo& GetSingleton();
        
        void Refresh();

        unsigned short getMonitorSequentialNumberFromHMonitor(HMONITOR hMonitor, bool allowRefresh = false);
        const MONITORINFO * const getMonitorInfo(unsigned short monitorIndex, bool allowRefresh = false);
        const unsigned short getMonitorsCount() const;
        RECT getBoundingMonitorArea();
    private:
        typedef std::map<HMONITOR, unsigned short> MapMonitorToSequentialNumber;
        typedef std::vector<MONITORINFO> ListMonitorInfo;

        MapMonitorToSequentialNumber mMapMonitors;
        ListMonitorInfo mListMonitorInfo;
        unsigned short mMonitorsCount;

        static BOOL CALLBACK MonitorEnumProc(
            _In_  HMONITOR hMonitor,
            _In_  HDC hdcMonitor,
            _In_  LPRECT lprcMonitor,
            _In_  LPARAM dwData
        );
    };

}
