#include "MonitorInfo.h"
namespace OIV
{
    MonitorInfo MonitorInfo::sInstance;

    MonitorInfo& MonitorInfo::GetSingleton()
    {
        return sInstance;
    }

    //---------------------------------------------------------------------
    MonitorInfo::MonitorInfo()
    {
        Refresh();
    }
    //---------------------------------------------------------------------
    void MonitorInfo::Refresh()
    {
        mMapMonitors.clear();
        mListMonitorInfo.clear();
        mMonitorsCount = 0;
        EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, (LPARAM)this);
    }
    //---------------------------------------------------------------------
    const MONITORINFO * const MonitorInfo::getMonitorInfo(unsigned short monitorIndex, bool allowRefresh /*= false*/)
    {
        LPMONITORINFO result = NULL;
        if (monitorIndex >= mListMonitorInfo.size() && allowRefresh)
            Refresh();

        if (monitorIndex < mListMonitorInfo.size())
            result = &mListMonitorInfo[monitorIndex];

        return result;
    }
    //---------------------------------------------------------------------
    unsigned short MonitorInfo::getMonitorSequentialNumberFromHMonitor(HMONITOR hMonitor, bool allowRefresh /*= false*/)
    {
        MapMonitorToSequentialNumber::const_iterator it = mMapMonitors.find(hMonitor);
        bool found = it != mMapMonitors.end();
        if (!found && allowRefresh)
        {
            Refresh();
            it = mMapMonitors.find(hMonitor);
            found = it != mMapMonitors.end();
        }

        if (found)
            return it->second;
        else
            return -1;
    }
    //---------------------------------------------------------------------
    BOOL CALLBACK MonitorInfo::MonitorEnumProc(_In_ HMONITOR hMonitor, _In_ HDC hdcMonitor, _In_ LPRECT lprcMonitor, _In_ LPARAM dwData)
    {
        MonitorInfo* _this = (MonitorInfo*)dwData;
        MONITORINFO monitorInfo;
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfo(hMonitor, &monitorInfo);
        _this->mListMonitorInfo.push_back(monitorInfo);
        _this->mMapMonitors.insert(std::make_pair(hMonitor, _this->mMonitorsCount++));
        return true;
    }
    //---------------------------------------------------------------------
    const unsigned short MonitorInfo::getMonitorsCount() const
    {
        return mMonitorsCount;
    }

    RECT MonitorInfo::getBoundingMonitorArea()
    {
        using namespace std;
        RECT rect = { 0 };
        int count = getMonitorsCount();
        for (int i = 0; i < count; i++)
        {
            const MONITORINFO* info = getMonitorInfo(i);
            const RECT& monRect = info->rcMonitor;
            rect.left = min(rect.left, monRect.left);
            rect.top = min(rect.top, monRect.top);
            rect.right = max(rect.right, monRect.right);
            rect.bottom = max(rect.bottom, monRect.bottom);
        }
        return rect;
    }
}