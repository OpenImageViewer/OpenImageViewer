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
    void ViewerApplication::InitializeNotificationIcons()
    {
        fNotificationIconID = 0;
    }

    LWS::Rect ViewerApplication::GetNotificationIconRect(
        [[maybe_unused]] LWS::NotificationIconGroup::IconID iconId) const
    {
        LL_EXCEPTION_NOT_IMPLEMENT("Notification icons are not implemented on Linux");
    }

    LWS::Handle ViewerApplication::FindTrayBarWindow()
    {
        return 0;
    }

    void ViewerApplication::InitializeShellState()
    {
        fNativeWindowState = std::make_unique<NativeWindowState>();
    }
}  // namespace OIV
