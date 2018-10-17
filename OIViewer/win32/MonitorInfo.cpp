#include "MonitorInfo.h"
#include <algorithm>
#include <shellscalingapi.h>

namespace OIV
{
    //---------------------------------------------------------------------
    MonitorInfo::MonitorInfo()
    {
        Refresh();
    }
    //---------------------------------------------------------------------
    void MonitorInfo::Refresh()
    {
        mDisplayDevices.clear();
        mHMonitorToDesc.clear();
        DISPLAY_DEVICE disp;
        disp.cb = sizeof(disp);;
        DWORD devNum = 0;
        while (EnumDisplayDevices(nullptr, devNum++, &disp, 0) == TRUE)
        {
            if ((disp.StateFlags & DISPLAY_DEVICE_ACTIVE) == DISPLAY_DEVICE_ACTIVE) // only connected
            {
                mDisplayDevices.push_back(MonitorDesc());
                MonitorDesc& desc = mDisplayDevices.back();
                desc.DisplayInfo = disp;
                desc.DisplaySettings.dmSize = sizeof(desc.DisplaySettings.dmSize);
                EnumDisplaySettings(disp.DeviceName, ENUM_CURRENT_SETTINGS, &desc.DisplaySettings);
            }
        }
        EnumDisplayMonitors(nullptr, nullptr, MonitorEnumProc, (LPARAM)this);
    }
    //---------------------------------------------------------------------
    const MonitorDesc& MonitorInfo::getMonitorInfo(unsigned short monitorIndex, bool allowRefresh /*= false*/)
    {
        MonitorDesc* result = nullptr;
        if (monitorIndex >= mDisplayDevices.size() && allowRefresh)
            Refresh();

		return monitorIndex < mDisplayDevices.size() ? mDisplayDevices[monitorIndex] : mEmptyMonitorDesc;
    }
    //---------------------------------------------------------------------
    const MonitorDesc&  MonitorInfo::getMonitorInfo(HMONITOR hMonitor, bool allowRefresh )
    {
        if (allowRefresh)
            Refresh();

        auto it = mHMonitorToDesc.find(hMonitor);
        return it == mHMonitorToDesc.end() ? mEmptyMonitorDesc : it->second;

    }
    //---------------------------------------------------------------------
    BOOL CALLBACK MonitorInfo::MonitorEnumProc(_In_ HMONITOR hMonitor, _In_ HDC hdcMonitor, _In_ LPRECT lprcMonitor, _In_ LPARAM dwData)
    {
        MonitorInfo* _this = (MonitorInfo*)dwData;
        MONITORINFOEX monitorInfo;
        monitorInfo.cbSize = sizeof(monitorInfo);
        GetMonitorInfo(hMonitor, &monitorInfo);
        for (MonitorDesc& desc : _this->mDisplayDevices)
        {
            if (std::wstring(desc.DisplayInfo.DeviceName) == monitorInfo.szDevice)
            {
                desc.monitorInfo = monitorInfo;
                desc.handle = hMonitor;

                UINT dpix;
                UINT dpiy;
                GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy);
                desc.DPIx = dpix;
                desc.DPIy = dpiy;

                _this->mHMonitorToDesc.insert(std::make_pair(hMonitor, desc));
                break;
            }
        }

        return true;
    }
    //---------------------------------------------------------------------
    const unsigned short MonitorInfo::getMonitorsCount() const
    {
        return mDisplayDevices.size();
    }

    RECT MonitorInfo::getBoundingMonitorArea()
    {
        using namespace std;
        RECT rect = { 0 };
        int count = getMonitorsCount();
        for (int i = 0; i < count; i++)
        {
            const MONITORINFOEX&  info = getMonitorInfo(i).monitorInfo;
            const RECT& monRect = info.rcMonitor;
            rect.left = min(rect.left, monRect.left);
            rect.top = min(rect.top, monRect.top);
            rect.right = max(rect.right, monRect.right);
            rect.bottom = max(rect.bottom, monRect.bottom);
        }
        return rect;
    }
}
