#include "HostedWindowController.h"
#include "MainWindow.h"
#include <LWS/Win32/WindowExtensions.hpp>
#include <Windows.h>
#include <LWS/Win32/ScopedWindowDpiContext.hpp>
namespace OIV::Win32
{
    LWS::Result HostedWindowController::Destroy()
    {
        // Native destruction can activate the shell synchronously. Disable focus forwarding first.
        escaped           = false;
        const auto result = owner.GetWindow().Destroy();
        if (fullscreen)
            std::ignore = fullscreen->Destroy();
        return result;
    }
    LWS::WindowMode HostedWindowController::GetWindowMode() const
    {
        return escaped ? fullscreen->GetWindowMode() : owner.GetWindow().GetWindowMode();
    }
    LWS::Result HostedWindowController::SetWindowMode(LWS::WindowMode mode)
    {
        auto& state = *this;
        if (mode != LWS::WindowMode::Windowed)
        {
            if (!state.fullscreen)
            {
                state.fullscreen = std::make_unique<LWS::Window>(owner.GetWindow().GetPlatformContext());
                const LWS::Win32::ScopedWindowDpiContext dpiContext(*LWS::Win32::GetHwnd(owner.GetWindow()));
                const auto result = state.fullscreen->Create({.title = LLUTILS_TEXT("OIViewer")});
                if (result != LWS::Result::Success)
                {
                    state.fullscreen.reset();
                    return result;
                }
                const HWND shell = *LWS::Win32::GetHwnd(*state.fullscreen);
                SetWindowLongPtrW(shell, GWLP_HWNDPARENT,
                                  reinterpret_cast<LONG_PTR>(
                                      GetAncestor(*LWS::Win32::GetHwnd(owner.GetWindow()), GA_ROOT)));
                auto connection = state.fullscreen->Listen(
                    [this](const LWS::AnyEvent& event)
                    {
                        if (std::holds_alternative<LWS::EventCloseRequested>(event))
                        {
                            std::ignore = SetWindowMode(LWS::WindowMode::Windowed);
                            return LWS::EventResponse::Handled;
                        }
                        if (escaped && std::holds_alternative<LWS::EventClientAreaSizeChanged>(event))
                            owner.FitToParent();
                        if (escaped && std::holds_alternative<LWS::EventFocusGained>(event))
                            SetFocus(*LWS::Win32::GetHwnd(owner.GetWindow()));
                        return LWS::EventResponse::Unhandled;
                    });
                if (!connection)
                {
                    state.fullscreen.reset();
                    return LWS::Result::Failure;
                }
                state.fullscreenConnection = std::move(*connection);
            }
            if (!state.escaped)
            {
                state.dockParent = owner.GetWindow().GetParent();
                const auto hwnd  = *LWS::Win32::GetHwnd(owner.GetWindow());
                RECT rect{};
                GetWindowRect(hwnd, &rect);
                SetWindowPos(*LWS::Win32::GetHwnd(*state.fullscreen), nullptr, rect.left, rect.top,
                             rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
                const auto result = owner.GetWindow().Reparent(*state.fullscreen);
                if (result != LWS::Result::Success)
                    return result;
                state.escaped = true;
            }
            const auto result = state.fullscreen->SetWindowMode(mode);
            if (result != LWS::Result::Success)
            {
                std::ignore = SetWindowMode(LWS::WindowMode::Windowed);
                return result;
            }
            std::ignore = state.fullscreen->SetVisible(true);
            std::ignore = state.fullscreen->RequestActivation();
        }
        else if (state.escaped)
        {
            if (state.dockParent == nullptr || !state.dockParent->IsCreated())
                return LWS::Result::InvalidState;
            const auto result = owner.GetWindow().Reparent(*state.dockParent);
            if (result != LWS::Result::Success)
                return result;
            state.escaped = false;
            // Reset the mode so the next entry reapplies fullscreen geometry. Restoring the saved
            // placement can show the shell again, so hide it only after the mode transition.
            std::ignore = state.fullscreen->SetWindowMode(LWS::WindowMode::Windowed);
            std::ignore = state.fullscreen->SetVisible(false);
        }
        owner.FitToParent();
        SetFocus(*LWS::Win32::GetHwnd(owner.GetWindow()));
        return LWS::Result::Success;
    }

}  // namespace OIV::Win32
