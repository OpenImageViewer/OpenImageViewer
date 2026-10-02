#pragma once

#include "ViewerApplication.h"

#include <LInput/Keys/KeyBindings.h>
#include <LInput/Win32/RawInput/RawInput.h>

#include <LWS/NotificationIconGroup.hpp>
#include <LWS/Win32/EventWin32.hpp>

#include <Windows.h>

#include <optional>

namespace OIV
{
    struct ViewerApplication::RawInputState
    {
        struct BindingElement
        {
            std::string commandDescription;
            std::string command;
            std::string arguments;
        };

        explicit RawInputState(ViewerApplication& application) : owner(application) {}

        void OnRawInput(const LInput::RawInput::RawInputEvent& event);
        void HandleKeyInput(const LWS::Win32::KeyEvent& event);
        std::optional<LRESULT> HandlePlatformEvent(const LWS::Win32::PlatformEvent& event);

        ViewerApplication& owner;
        LInput::RawInput rawInput;
        LInput::RawInput::OnInputType::Subscription inputSubscription;
        LInput::KeyBindings<BindingElement> keyBindings;
    };

    struct ViewerApplication::NativeWindowState
    {
        explicit NativeWindowState(LWS::PlatformContext& platform) : notificationIcons(platform) {}

        LWS::NotificationIconGroup notificationIcons;
        LWS::NotificationIconGroup::NotificationIconEvent::Subscription notificationSubscription;
        HMODULE settingsModule                    = nullptr;
        DLL_DIRECTORY_COOKIE settingsDllDirectory = nullptr;
    };
}  // namespace OIV
