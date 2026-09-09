#include "ViewerApplication.h"

#include "ViewerApplicationPlatformState.h"

#include <LLUtils/Exception.h>
#include <LLUtils/PlatformUtility.h>
#include <OIVAppCore/ViewCommandPolicy.h>

#include <cstdlib>
#include <filesystem>

#ifdef LWS_HAS_WAYLAND_BACKEND
    #include <LWS/Wayland/WindowExtensions.hpp>
#endif

namespace OIV
{
    LLUtils::native_string_type ViewerApplication::GetAppDataFolder()
    {
        const char* configHome = std::getenv("XDG_CONFIG_HOME");
        std::filesystem::path folder;
        if (configHome != nullptr && *configHome != '\0')
        {
            folder = configHome;
        }
        else if (const char* userHome = std::getenv("HOME"); userHome != nullptr && *userHome != '\0')
        {
            folder = std::filesystem::path(userHome) / ".config";
        }
        else
        {
            folder = std::filesystem::temp_directory_path();
        }
        return (folder / "OIV").native() + LLUTILS_TEXT("/");
    }

    LWS::Handle ViewerApplication::FindTrayBarWindow()
    {
        return 0;
    }

    LLUtils::native_string_type ViewerApplication::GetApplicationModulePath()
    {
        return LLUtils::PlatformUtility::GetExePath();
    }

    void ViewerApplication::InitializePlatformState()
    {
        fRawInputState.reset(new RawInputState());
        fNativeWindowState.reset(new NativeWindowState());
        fFileWatcher.reset();
        fRenderGateway = std::make_unique<OivRenderGateway>(OivRenderGateway::PresentationState::Deferred);
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
        fNotificationIconID = 0;
    }

    void ViewerApplication::InitializeRenderer(const RendererOptions& rendering)
    {
        auto& canvas = fWindow.GetCanvasWindow();
        fRenderGateway->Initialize(reinterpret_cast<LWS::Handle>(*LWS::Wayland::GetSurface(canvas)),
                                   *LWS::Wayland::GetDisplay(canvas), rendering);
    }

    WindowSizeDecision ViewerApplication::GetWindowSizeDecision(const CommandManager::CommandArgs& args) const
    {
        const auto& workRect               = fCurrentMonitorProperties.workRect;
        const auto workAreaTopLeft         = workRect.GetCorner(LLUtils::TopLeft);
        const auto workAreaBottomRight     = workRect.GetCorner(LLUtils::BottomRight);
        const WindowWorkingArea workArea   = {.left   = workAreaTopLeft.x,
                                              .top    = workAreaTopLeft.y,
                                              .right  = workAreaBottomRight.x,
                                              .bottom = workAreaBottomRight.y};
        const LWS::LogicalSize currentSize = fWindow.GetWindow().GetClientSize();
        return ViewCommandPolicy::DecideWindowSize(args, {currentSize.x, currentSize.y},
                                                   fWindow.GetWindow().GetPosition().value_or(LWS::Point{}), workArea);
    }

    LWS::Rect ViewerApplication::GetNotificationIconRect(
        [[maybe_unused]] LWS::NotificationIconGroup::IconID iconId) const
    {
        LL_EXCEPTION_NOT_IMPLEMENT("Notification icons are not implemented on Linux");
    }

    void ViewerApplication::Run()
    {
        fPlatform.RunMessageLoop();
    }
}  // namespace OIV
