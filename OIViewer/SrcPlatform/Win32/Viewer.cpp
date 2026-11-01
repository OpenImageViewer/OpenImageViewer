#include "Viewer.h"

#include "FileWatcherWin32.h"
#include "Resource.h"
#include "ViewerPlatformState.h"

#include <LLUtils/Exception.h>
#include <LLUtils/PlatformUtility.h>
#include <OIVAppCore/ViewCommandPolicy.h>

#include <Windows.h>

#include <LWS/Win32/WindowExtensions.hpp>

#include <cmath>

namespace OIV
{
    std::unique_ptr<IFileWatcher> CreateViewerFileWatcher()
    {
        return std::make_unique<Win32::FileWatcherWin32>();
    }

    void Viewer::InitializePlatformState()
    {
        fRawInputState.reset(new RawInputState(*this));
    }

    void Viewer::RawInputStateDeleter::operator()(RawInputState* state) const noexcept
    {
        delete state;
    }

    void Viewer::InitializeRenderer(const RendererOptions& rendering)
    {
        const auto canvasHandle = LWS::Win32::GetHwnd(fWindow.GetCanvasWindow());
        if (!canvasHandle.has_value())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to obtain the canvas window handle");
        auto created  = CreateRenderer({reinterpret_cast<LWS::Handle>(*canvasHandle), nullptr},
                                       fUserDirectories.GeneratedCache(), rendering);
        fRenderer     = std::move(created.renderer);
        fRendererType = created.type;
    }

    WindowSizeDecision Viewer::GetWindowSizeDecision(const CommandManager::CommandArgs& args) const
    {
        const auto& window = fWindow.GetWindow();
        const HWND handle  = *LWS::Win32::GetHwnd(window);
        RECT windowRect{};
        MONITORINFO monitor{sizeof(MONITORINFO)};
        if (!GetWindowRect(handle, &windowRect) ||
            !GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &monitor))
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to obtain window sizing geometry");

        // LWS publishes native DPI before dispatching geometry changes and supplies legacy fallbacks.
        const auto dpi = LWS::Win32::GetDpi(window);
        if (!dpi)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to obtain window DPI");
        const double scale   = *dpi / 96.0;
        const auto toLogical = [scale](LLUtils::PointI32 point)
        {
            return LLUtils::PointI32{static_cast<int32_t>(std::lround(point.x / scale)),
                                     static_cast<int32_t>(std::lround(point.y / scale))};
        };
        const auto topLeft     = toLogical({monitor.rcWork.left, monitor.rcWork.top});
        const auto bottomRight = toLogical({monitor.rcWork.right, monitor.rcWork.bottom});
        const auto position    = toLogical({windowRect.left, windowRect.top});
        const auto size        = window.GetClientAreaMetrics().logical;
        return ViewCommandPolicy::DecideWindowSize(args, {size.x, size.y}, position,
                                                   {topLeft.x, topLeft.y, bottomRight.x, bottomRight.y});
    }

}  // namespace OIV
