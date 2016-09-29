#pragma once
#include "OgreSingleton.h"
#include <windows.h>
namespace OIV
{
    class MonitorInfo : public Ogre::Singleton<MonitorInfo>
    {
    public:
        MonitorInfo();
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
