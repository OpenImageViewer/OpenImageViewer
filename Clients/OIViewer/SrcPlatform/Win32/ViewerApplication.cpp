#include "ViewerApplication.h"
#include "ViewerApplicationPlatformState.h"

#include "FileWatcherWin32.h"
#include "Resource.h"

#include <LLUtils/Exception.h>
#include <LLUtils/PlatformUtility.h>
#include <OIVAppCore/ViewCommandPolicy.h>

#include <Windows.h>

#include <LWS/Win32/WindowExtensions.hpp>

#include <cmath>

namespace OIV
{
    void ViewerApplication::InitializeNotificationIcons()
    {
        fNotificationIconID = fNativeWindowState->notificationIcons.AddIconResource(IDI_APP_ICON,
                                                                                    LLUTILS_TEXT("Open Image Viewer"));
        fNativeWindowState->notificationSubscription =
            fNativeWindowState->notificationIcons.OnNotificationIconEvent.Subscribe(
                std::bind(&ViewerApplication::OnNotificationIcon, this, std::placeholders::_1));
    }

    LWS::Rect ViewerApplication::GetNotificationIconRect(LWS::NotificationIconGroup::IconID iconId) const
    {
        return fNativeWindowState->notificationIcons.GetIconRect(iconId);
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

    void ViewerApplication::InitializeShellState()
    {
        fNativeWindowState = std::make_unique<NativeWindowState>(fPlatform);
    }
}  // namespace OIV
