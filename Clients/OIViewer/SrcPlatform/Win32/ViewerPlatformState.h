#pragma once

#include "Viewer.h"

#include <LInput/Keys/KeyBindings.h>
#include <LInput/Win32/RawInput/RawInput.h>

#include <LWS/Win32/EventWin32.hpp>

#include <Windows.h>

#include <bitset>
#include <optional>

namespace OIV
{
    struct Viewer::RawInputState
    {
        struct BindingElement
        {
            std::string commandDescription;
            std::string command;
            std::string arguments;
        };

        explicit RawInputState(Viewer& application) : owner(application) {}

        void OnRawInput(const LInput::RawInput::RawInputEvent& event);
        bool HandleKeyInput(const LWS::Win32::KeyEvent& event);
        std::optional<LRESULT> HandlePlatformEvent(const LWS::Win32::PlatformEvent& event);

        Viewer& owner;
        LInput::RawInput rawInput;
        LInput::RawInput::OnInputType::Subscription inputSubscription;
        LInput::KeyBindings<BindingElement> keyBindings;
        std::bitset<256> hostKeys;
    };

}  // namespace OIV
