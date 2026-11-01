#pragma once
#include "ViewerApplication.h"
#include <Windows.h>

namespace OIV
{
    struct ViewerApplication::NativeWindowState
    {
        explicit NativeWindowState(LWS::PlatformContext& platform) : notificationIcons(platform) {}

        LWS::NotificationIconGroup notificationIcons;
        LWS::NotificationIconGroup::NotificationIconEvent::Subscription notificationSubscription;
        HWND associationDialog{};
    };
}  // namespace OIV
