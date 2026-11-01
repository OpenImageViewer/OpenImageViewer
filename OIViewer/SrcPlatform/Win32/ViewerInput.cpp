#include "Viewer.h"

#include "Globals.h"
#include "CopyDataProtocol.h"
#include "ViewerPlatformState.h"
#include "ViewerMouseInput.h"

#include <LInput/Keys/KeyCombination.h>
#include <LWS/Win32/EventWin32.hpp>
#include <LWS/Win32/WindowExtensions.hpp>

#include <LLUtils/Exception.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>

#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <OIVAppCore/ConfigurationLoader.h>

#include <Windows.h>

namespace OIV
{
    void Viewer::RawInputState::OnRawInput(const LInput::RawInput::RawInputEvent& event)
    {
        using namespace LInput;
        if (event.deviceType != RawInput::RawInputDeviceType::Mouse)
            return;

        const auto& mouse = static_cast<const RawInput::RawInputEventMouse&>(event);
        constexpr std::array buttons{LWS::MouseButton::Left, LWS::MouseButton::Right, LWS::MouseButton::Middle,
                                     LWS::MouseButton::X1, LWS::MouseButton::X2};
        for (size_t index = 0; index < buttons.size(); ++index)
        {
            if (mouse.buttonState[index] == ButtonState::Up)
                owner.fMouseInput->SetButton(buttons[index], false, false);
        }
        owner.fMouseInput->Move({mouse.deltaX, mouse.deltaY});
        if (mouse.wheelDelta != 0)
        {
            // Use the viewer subtree rather than the process's top-level window: embedded canvases belong to
            // the host's root. Overlying windows still block uncaptured wheel actions.
            POINT cursor{};
            const HWND window      = LWS::Win32::GetHwnd(owner.fWindow.GetWindow()).value_or(nullptr);
            const HWND hit         = window != nullptr && GetCursorPos(&cursor) ? WindowFromPoint(cursor) : nullptr;
            const bool mouseInside = window != nullptr && (hit == window || IsChild(window, hit)) &&
                                     owner.fWindow.GetCanvasWindow().IsMouseInClientRect();
            owner.fMouseInput->Wheel(static_cast<double>(mouse.wheelDelta) / LWS::EventMouseWheel::DeltaPerStep,
                                     mouseInside);
        }
    }
    void Viewer::InitializeRawInput()
    {
        using namespace LInput;
        auto connection = LWS::Win32::Listen(fWindow.GetWindow(),
                                             [this](const LWS::Win32::PlatformEvent& event)
                                             {
                                                 std::optional<LRESULT> result;
                                                 HandleEventCallback(
                                                     [&]()
                                                     {
                                                         result = fRawInputState->HandlePlatformEvent(event);
                                                         return false;
                                                     });
                                                 return result;
                                             });
        if (!connection.has_value())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to register required window listener");
        fPlatformConnection = std::move(*connection);
    }

    void Viewer::EnableRawMouse()
    {
        // Only the standalone frontend claims process-wide raw input. Hosts retain their own registration.
        using namespace LInput;
        fRawInputState->rawInput.AddDevice(RawInput::UsagePage::GenericDesktopControls,
                                           RawInput::GenericDesktopControlsUsagePage::Mouse,
                                           RawInput::Flags::EnableBackground);
        fRawInputState->inputSubscription = fRawInputState->rawInput.OnInput.Subscribe(

            [this](const RawInput::RawInputEvent& event)
            {
                HandleEventCallback(
                    [&]()
                    {
                        fRawInputState->OnRawInput(event);
                        return true;
                    });
            });
        fRawInputState->rawInput.Enable(true);
    }

    int Viewer::GetRawNavigationDirection() const
    {
        return fMouseInput->GetNavigationDirection();
    }

    void Viewer::AddPlatformKeyBindings()
    {
        for (const auto& keyBinding : fConfiguredBindings)
        {
            fRawInputState->keyBindings.AddBinding(LInput::KeyCombination::FromString(keyBinding.KeyCombinationName),
                                                   {keyBinding.GroupID, std::string(), std::string()});
        }
    }

    bool Viewer::RawInputState::HandleKeyInput(const LWS::Win32::KeyEvent& eventData)
    {
        if (!eventData.pressed)
            return false;

        const LInput::KeyCombination keyCombination = LInput::KeyCombination::FromVirtualKey(eventData.virtualKey,
                                                                                             eventData.keyData);
        LInput::KeyBindings<RawInputState::BindingElement>::ConcreteBindingType bindings;
        const bool matched = keyBindings.GetBinding(keyCombination, bindings);
        if (matched)
        {
            for (const auto& binding : bindings)
                owner.ExecutePredefinedCommand(binding.commandDescription);
        }
        return matched;
    }

    std::intptr_t Viewer::ClientWindwMessage(const LWS::AnyEvent& eventData)
    {
        if (const auto* button = std::get_if<LWS::EventMouseButton>(&eventData))
        {
            if (fShell.forwardKey && button->pressed)
                SetFocus(*LWS::Win32::GetHwnd(fWindow.GetWindow()));
            fMouseInput->SetButton(button->button, button->pressed, true);
            return 1;
        }
        if (fShell.forwardKey)
        {
            if (const auto* motion = std::get_if<LWS::EventMouseMove>(&eventData))
            {
                fMouseInput->Move(motion->delta);
                UpdateTexelPos();
                return 1;
            }
            if (const auto* wheel = std::get_if<LWS::EventMouseWheel>(&eventData))
            {
                const auto size   = fWindow.GetCanvasWindow().GetClientAreaMetrics().logical;
                const bool inside = wheel->position.x >= 0 && wheel->position.y >= 0 && wheel->position.x < size.x &&
                                    wheel->position.y < size.y;
                fMouseInput->Wheel(wheel->steps(), inside);
                return 1;
            }
        }
        return 0;
    }

    std::optional<LRESULT> Viewer::RawInputState::HandlePlatformEvent(const LWS::Win32::PlatformEvent& eventData)
    {
        if (const auto* key = std::get_if<LWS::Win32::KeyEvent>(&eventData))
        {
            if (key->pressed && key->virtualKey == VK_ESCAPE && (key->keyData & (1U << 30)) != 0)
                return 0;
            // Dismiss the selection before Escape can close or return to the host.
            if (key->pressed && key->virtualKey == VK_ESCAPE &&
                owner.fSelectionRect.GetOperation() != SelectionRect::Operation::NoOp)
            {
                owner.fMouseInput->Cancel();
                owner.CancelSelection();
                return 0;
            }
            if (key->virtualKey == VK_MENU || key->virtualKey == VK_LMENU || key->virtualKey == VK_RMENU)
                owner.UpdateSelectionCursor(key->pressed);
            if (owner.fShell.forwardKey && key->pressed && key->virtualKey == VK_ESCAPE)
            {
                owner.CloseApplication(false);
                return 0;
            }
            if (!key->pressed)
            {
                const auto keyCode =
                    LInput::KeyCombination::FromVirtualKey(key->virtualKey, key->keyData).keydata().keycode;
                if (keyCode == LInput::KeyCode::LALT || keyCode == LInput::KeyCode::RIGHTALT ||
                    keyCode == LInput::KeyCode::RALT)
                    owner.fDoubleTap.SetState(false);
            }
            if (owner.fShell.forwardKey && key->virtualKey < hostKeys.size())
            {
                // Bindings claim keys even if their command fails. Keep the initial recipient through
                // repeats and release so changing modifiers cannot send half a gesture to the host.
                const bool repeat = (key->keyData & (1U << 30)) != 0;
                if (key->pressed && !repeat)
                    hostKeys.set(key->virtualKey, !HandleKeyInput(*key));
                else if (key->pressed && !hostKeys[key->virtualKey])
                    HandleKeyInput(*key);
                const bool forward = hostKeys[key->virtualKey];
                if (!key->pressed)
                    hostKeys.reset(key->virtualKey);
                if (forward)
                {
                    // Use the stable host container even in fullscreen. Posting lets this callback
                    // finish before a host command can close the viewer or unload the plugin.
                    owner.fShell.forwardKey(key->message, key->virtualKey, key->keyData);
                }
                return 0;
            }
            HandleKeyInput(*key);
            return std::nullopt;
        }
        if (const auto* activation = std::get_if<LWS::Win32::ActivationEvent>(&eventData))
        {
            if (!activation->active)
                owner.fMouseInput->Cancel();
            else
                owner.fWindow.SetIsTrayWindow(false);
            owner.SetAppActive(activation->active);
            return std::nullopt;
        }
        if (const auto* copyData = std::get_if<LWS::Win32::CopyDataEvent>(&eventData);
            owner.fShell.openExternalFile && copyData != nullptr && copyData->identifier == Win32::LoadFileCopyDataId &&
            copyData->data.size() >= sizeof(wchar_t) && copyData->data.size() % sizeof(wchar_t) == 0)
        {
            const auto* fileToLoad      = reinterpret_cast<const wchar_t*>(copyData->data.data());
            const size_t characterCount = copyData->data.size() / sizeof(wchar_t);
            if (fileToLoad[characterCount - 1] == L'\0')
            {
                owner.fShell.openExternalFile(fileToLoad);
                return TRUE;
            }
        }
        return std::nullopt;
    }

    bool Viewer::HandleWinMessageEvent(const LWS::AnyEvent& eventData)
    {
        // Native wheels target the focused composite, while capture can target either composite or canvas.
        if (fShell.forwardKey && (std::holds_alternative<LWS::EventMouseMove>(eventData) ||
                                  std::holds_alternative<LWS::EventMouseWheel>(eventData)))
            return ClientWindwMessage(eventData) != 0;
        if (const auto* button = std::get_if<LWS::EventMouseButton>(&eventData))
        {
            const auto canvasSize  = fWindow.GetCanvasWindow().GetClientAreaMetrics().logical;
            const bool mouseInside = button->position.x >= 0 && button->position.y >= 0 &&
                                     button->position.x < canvasSize.x && button->position.y < canvasSize.y;
            fMouseInput->SetButton(button->button, button->pressed, mouseInside);
            return mouseInside || !button->pressed;
        }
        if (std::holds_alternative<LWS::EventMove>(eventData))
            fMonitorProvider.UpdateFromWindow(fWindow.GetWindow());
        else if (std::holds_alternative<LWS::EventMouseMove>(eventData))
            UpdateTexelPos();
        else if (std::holds_alternative<LWS::EventPaint>(eventData) && !fIsFirstFrameDisplayed)
        {
            CompleteInitialization();
        }
        return false;
    }

}  // namespace OIV
