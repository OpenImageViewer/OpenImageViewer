#include "ViewerApplication.h"

#include "Globals.h"
#include "CopyDataProtocol.h"
#include "ViewerApplicationPlatformState.h"
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
    void ViewerApplication::RawInputState::OnRawInput(const LInput::RawInput::RawInputEvent& event)
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
            // Background raw input also arrives when another window covers the canvas. Compare root windows so
            // the viewer's canvas child is accepted, while an overlying window blocks uncaptured wheel actions.
            POINT cursor{};
            const HWND window      = LWS::Win32::GetHwnd(owner.fWindow.GetWindow()).value_or(nullptr);
            const bool mouseInside = window != nullptr && GetCursorPos(&cursor) != FALSE &&
                                     GetAncestor(WindowFromPoint(cursor), GA_ROOT) == window &&
                                     owner.fWindow.GetCanvasWindow().IsMouseInClientRect();
            owner.fMouseInput->Wheel(static_cast<double>(mouse.wheelDelta) / LWS::EventMouseWheel::DeltaPerStep,
                                     mouseInside);
        }
    }
    void ViewerApplication::InitializeRawInput()
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
        if (connection.has_value())
            fPlatformConnection = std::move(*connection);
        fRawInputState->rawInput.AddDevice(RawInput::UsagePage::GenericDesktopControls,
                                           RawInput::GenericDesktopControlsUsagePage::Mouse,
                                           RawInput::Flags::EnableBackground);
        fRawInputState->rawInput.OnInput.Add(
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

    int ViewerApplication::GetRawNavigationDirection() const
    {
        return fMouseInput->GetNavigationDirection();
    }

    void ViewerApplication::AddPlatformKeyBindings()
    {
        for (const auto& keyBinding : ConfigurationLoader::LoadKeyBindings())
        {
            fRawInputState->keyBindings.AddBinding(LInput::KeyCombination::FromString(keyBinding.KeyCombinationName),
                                                   {keyBinding.GroupID, std::string(), std::string()});
        }
    }

    void ViewerApplication::RawInputState::HandleKeyInput(const LWS::Win32::KeyEvent& eventData)
    {
        if (!eventData.pressed)
            return;

        const LInput::KeyCombination keyCombination = LInput::KeyCombination::FromVirtualKey(eventData.virtualKey,
                                                                                             eventData.keyData);
        LInput::KeyBindings<RawInputState::BindingElement>::ConcreteBindingType bindings;
        if (keyBindings.GetBinding(keyCombination, bindings))
        {
            for (const auto& binding : bindings)
                owner.ExecutePredefinedCommand(binding.commandDescription);
        }
    }

    std::intptr_t ViewerApplication::ClientWindwMessage(const LWS::AnyEvent& eventData)
    {
        if (const auto* button = std::get_if<LWS::EventMouseButton>(&eventData))
        {
            fMouseInput->SetButton(button->button, button->pressed, true);
            return 1;
        }
        if (std::holds_alternative<LWS::EventClientAreaSizeChanged>(eventData))
        {
            fRefreshOperation.Begin();
            UpdateWindowSize();
            fRefreshOperation.End();
        }
        return 0;
    }

    std::optional<LRESULT> ViewerApplication::RawInputState::HandlePlatformEvent(
        const LWS::Win32::PlatformEvent& eventData)
    {
        if (const auto* key = std::get_if<LWS::Win32::KeyEvent>(&eventData))
        {
            if (!key->pressed)
            {
                const auto keyCode =
                    LInput::KeyCombination::FromVirtualKey(key->virtualKey, key->keyData).keydata().keycode;
                if (keyCode == LInput::KeyCode::LALT || keyCode == LInput::KeyCode::RIGHTALT ||
                    keyCode == LInput::KeyCode::RALT)
                    owner.fDoubleTap.SetState(false);
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
            copyData != nullptr && copyData->identifier == Win32::LoadFileCopyDataId &&
            copyData->data.size() >= sizeof(wchar_t) && copyData->data.size() % sizeof(wchar_t) == 0)
        {
            const auto* fileToLoad      = reinterpret_cast<const wchar_t*>(copyData->data.data());
            const size_t characterCount = copyData->data.size() / sizeof(wchar_t);
            if (fileToLoad[characterCount - 1] == L'\0')
            {
                owner.LoadFile(fileToLoad, IMCodec::PluginTraverseMode::NoTraverse);
                std::ignore = owner.fWindow.GetWindow().SetVisible(true);
                return TRUE;
            }
        }
        return std::nullopt;
    }

    bool ViewerApplication::HandleWinMessageEvent(const LWS::AnyEvent& eventData)
    {
        if (const auto* button = std::get_if<LWS::EventMouseButton>(&eventData))
        {
            const auto canvasSize  = fWindow.GetCanvasWindow().GetClientSize();
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
            fIsFirstFrameDisplayed = true;
            AfterFirstFrameDisplayed();
        }
        return false;
    }

    void ViewerApplication::CloseApplication(bool closeToTray)
    {
        const HANDLE mutex = CreateMutex(
            nullptr, FALSE, (LLUtils::native_string_type(Globals::ProgramGuid) + LLUTILS_TEXT("_CLOSEAPP")).c_str());
        if (mutex == nullptr)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be created.");

        const DWORD result = WaitForSingleObject(mutex, INFINITE);
        if (result != WAIT_OBJECT_0)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex ownership cannot be acquired.");

        std::ignore = fWindow.GetWindow().SetVisible(false);
        if (!closeToTray || FindTrayBarWindow() != 0)
            fPlatform.RequestQuit();
        else
            fWindow.SetIsTrayWindow(true);

        if (!ReleaseMutex(mutex))
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be released.");
        if (!CloseHandle(mutex))
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Mutex cannot be closed.");
    }
}  // namespace OIV
