#include "MonitorProvider.h"

#include <LWS/Window.hpp>

#include <Windows.h>

#include <LWS/Win32/WindowExtensions.hpp>

namespace OIV
{
    void MonitorProvider::UpdateFromWindow(LWS::Window& window)
    {
        const HMONITOR monitor = MonitorFromWindow(*LWS::Win32::GetHwnd(window), MONITOR_DEFAULTTONULL);
        if (monitor != nullptr)
        {
            const uintptr_t monitorHandle = reinterpret_cast<uintptr_t>(monitor);
            if (monitorHandle != fMonitorDesc.handle)
            {
                auto& platform = window.GetPlatformContext();
                // LWS initializes this cache before window events. Future hot-plug support should refresh it from
                // WM_DISPLAYCHANGE, never from window-move handling.
                const auto monitorDesc = platform.GetMonitorInfo(monitorHandle, false);
                if (monitorDesc.has_value() && monitorDesc->handle == monitorHandle)
                {
                    fMonitorDesc = *monitorDesc;
                    Changed.Raise(fMonitorDesc);
                }
            }
        }
    }
}  // namespace OIV
