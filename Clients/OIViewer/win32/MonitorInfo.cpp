#include "MonitorInfo.h"
#include <algorithm>
#include <string>
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
        fBoundAreaOutOfDate = true;
        mDisplayDevices.clear();
        mHMonitorToDesc.clear();
        fPrimaryMonitorIterator = mHMonitorToDesc.end();
        
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
    const MonitorDesc& MonitorInfo::getMonitorInfo(size_t monitorIndex, bool allowRefresh /*= false*/)
    {
        if (monitorIndex >= mDisplayDevices.size() && allowRefresh)
            Refresh();

		return monitorIndex < mDisplayDevices.size() ? mDisplayDevices[monitorIndex] : mEmptyMonitorDesc;
    }
    //---------------------------------------------------------------------
    const MonitorDesc& MonitorInfo::GetPrimaryMonitor(bool allowRefresh)
    {
        if (allowRefresh)
            Refresh();
        return fPrimaryMonitorIterator->second;
    }
    //---------------------------------------------------------------------
    const MonitorDesc&  MonitorInfo::getMonitorInfo(HMONITOR hMonitor, bool allowRefresh )
    {
        if (allowRefresh)
            Refresh();

        auto it = mHMonitorToDesc.find(hMonitor);
        return it == mHMonitorToDesc.end() ? mEmptyMonitorDesc : it->second;

    }

    MonitorInfo::OSVersionInfo MonitorInfo::GetOSVersion()
    {
        HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");

        using NTSTATUS = LONG;
        constexpr NTSTATUS STATUS_SUCCESS = 0x00000000;
        using RtlGetVersionPtr = NTSTATUS(WINAPI*)(OSVersionInfo*);

        OSVersionInfo vi{};
        
        if (hMod != nullptr) 
        {
            RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
            if (fxPtr != nullptr) 
            {
                vi.dwOSVersionInfoSize = sizeof(vi);
                if (STATUS_SUCCESS != fxPtr(&vi))
                {
                    // error
                }
            }
        }
        return vi;
    }

    //---------------------------------------------------------------------
    BOOL CALLBACK MonitorInfo::MonitorEnumProc(_In_ HMONITOR hMonitor,[[maybe_unused]] _In_ HDC hdcMonitor, [[maybe_unused]] _In_ LPRECT lprcMonitor, _In_ LPARAM dwData)
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

                const static OSVersionInfo versionInfo = GetOSVersion();
                const static double windowsVersion = versionInfo.dwMajorVersion + versionInfo.dwMinorVersion / 10.0;

                
                if (windowsVersion >= 6.3)
                {
                    GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpix, &dpiy);
                }
                else
                {
                    HWND desktopWindow = GetDesktopWindow();
                    HDC hDC = ::GetDC(desktopWindow);
                    dpix = static_cast<UINT>(::GetDeviceCaps(hDC, LOGPIXELSX));
                    dpiy = static_cast<UINT>(::GetDeviceCaps(hDC, LOGPIXELSY));
                    ::ReleaseDC(desktopWindow, hDC);
                }

                desc.DPIx = static_cast<uint16_t>(dpix);
                desc.DPIy = static_cast<uint16_t>(dpiy);

                auto it = _this->mHMonitorToDesc.insert(std::make_pair(hMonitor, desc));
                
                if ((monitorInfo.dwFlags & MONITORINFOF_PRIMARY) == MONITORINFOF_PRIMARY)
                    _this->fPrimaryMonitorIterator = it.first;
                break;
            }
        }

        return true;
    }
    //---------------------------------------------------------------------
    size_t MonitorInfo::getMonitorsCount() const
    {
        return mDisplayDevices.size();
    }

    RECT MonitorInfo::getBoundingMonitorArea()
    {
        if (fBoundAreaOutOfDate == true)
        {
            fBoundArea = getBoundingMonitorAreaInternal();
            fBoundAreaOutOfDate = false;
        }
        return fBoundArea;
    }

    RECT MonitorInfo::getBoundingMonitorAreaInternal()
    {
        using namespace std;
        RECT rect {};
        size_t count = getMonitorsCount();
        for (size_t i = 0; i < count; i++)
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
