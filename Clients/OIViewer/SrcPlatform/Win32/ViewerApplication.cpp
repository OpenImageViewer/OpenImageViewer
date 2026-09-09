#include "ViewerApplication.h"

#include "FileWatcherWin32.h"
#include "Resource.h"
#include "ViewerApplicationPlatformState.h"

#include <LLUtils/Exception.h>
#include <LLUtils/PlatformUtility.h>
#include <OIVAppCore/ViewCommandPolicy.h>

#include <Windows.h>

#include <LWS/Win32/WindowExtensions.hpp>

#include <cmath>

namespace OIV
{
    LLUtils::native_string_type ViewerApplication::GetAppDataFolder()
    {
        return LLUtils::PlatformUtility::GetAppDataFolder() + LLUTILS_TEXT("/OIV/");
    }

    LWS::Handle ViewerApplication::FindTrayBarWindow()
    {
        HWND nextChild = nullptr;
        do
        {
            nextChild = FindWindowEx(nullptr, nextChild, nullptr, nullptr);
        } while (nextChild != nullptr && !MainWindow::GetIsTrayWindow(reinterpret_cast<LWS::Handle>(nextChild)));
        return reinterpret_cast<LWS::Handle>(nextChild);
    }

    LLUtils::native_string_type ViewerApplication::GetApplicationModulePath()
    {
        return LLUtils::PlatformUtility::GetDllPath();
    }

    void ViewerApplication::InitializePlatformState()
    {
        fRawInputState.reset(new RawInputState(*this));
        fNativeWindowState.reset(new NativeWindowState(fPlatform));
        fFileWatcher   = std::make_unique<Win32::FileWatcherWin32>();
        fRenderGateway = std::make_unique<OivRenderGateway>();
    }

    void ViewerApplication::RawInputStateDeleter::operator()(RawInputState* state) const noexcept
    {
        delete state;
    }

    void ViewerApplication::NativeWindowStateDeleter::operator()(NativeWindowState* state) const noexcept
    {
        delete state;
    }

    void ViewerApplication::InitializeNotificationIcons()
    {
        fNotificationIconID = fNativeWindowState->notificationIcons.AddIconResource(IDI_APP_ICON,
                                                                                    LLUTILS_TEXT("Open Image Viewer"));
        fNativeWindowState->notificationIcons.OnNotificationIconEvent.Add(
            std::bind(&ViewerApplication::OnNotificationIcon, this, std::placeholders::_1));
    }

    void ViewerApplication::InitializeRenderer(const RendererOptions& rendering)
    {
        const auto canvasHandle = LWS::Win32::GetHwnd(fWindow.GetCanvasWindow());
        if (!canvasHandle.has_value())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to obtain the canvas window handle");
        fRenderGateway->Initialize(reinterpret_cast<LWS::Handle>(*canvasHandle), nullptr, rendering);
    }

    WindowSizeDecision ViewerApplication::GetWindowSizeDecision(const CommandManager::CommandArgs& args) const
    {
        const auto& window = fWindow.GetWindow();
        const HWND handle  = *LWS::Win32::GetHwnd(window);
        RECT windowRect{};
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetWindowRect(handle, &windowRect) ||
            !GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor))
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to obtain window sizing geometry");

        // DPI and work-area changes need not change the monitor handle. Query current native geometry here.
        using GetDpiForWindowFn           = UINT(WINAPI*)(HWND);
        static const auto getDpiForWindow = reinterpret_cast<GetDpiForWindowFn>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
        const double scale   = getDpiForWindow != nullptr ? getDpiForWindow(handle) / 96.0
                                                          : fCurrentMonitorProperties.contentScale.x;
        const auto toLogical = [scale](LLUtils::PointI32 point)
        {
            return LLUtils::PointI32{static_cast<int32_t>(std::lround(point.x / scale)),
                                     static_cast<int32_t>(std::lround(point.y / scale))};
        };
        const auto topLeft     = toLogical({monitor.rcWork.left, monitor.rcWork.top});
        const auto bottomRight = toLogical({monitor.rcWork.right, monitor.rcWork.bottom});
        const auto position    = toLogical({windowRect.left, windowRect.top});
        const auto size        = window.GetClientSize();
        return ViewCommandPolicy::DecideWindowSize(args, {size.x, size.y}, position,
                                                   {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y});
    }

    LWS::Rect ViewerApplication::GetNotificationIconRect(LWS::NotificationIconGroup::IconID iconId) const
    {
        return fNativeWindowState->notificationIcons.GetIconRect(iconId);
    }

    void ViewerApplication::Run()
    {
        fPlatform.RunMessageLoop();
    }
}  // namespace OIV
