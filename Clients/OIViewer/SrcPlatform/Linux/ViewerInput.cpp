#include "Viewer.h"

#include <LWS/Platform.hpp>

#include "ViewerPlatformState.h"
#include "ViewerMouseInput.h"

#include <LLUtils/Exception.h>

#include <OIVAppCore/ConfigurationLoader.h>

namespace OIV
{
    void Viewer::InitializeRawInput() {}

    int Viewer::GetRawNavigationDirection() const
    {
        return fMouseInput->GetNavigationDirection();
    }

    void Viewer::AddPlatformKeyBindings()
    {
        for (const auto& binding : fConfiguredBindings)
            fRawInputState->keyBindings.AddBinding(binding.KeyCombinationName, binding.GroupID);
    }

    bool Viewer::handleKeyInput(const LWS::AnyEvent& eventData)
    {
        const auto* keyEvent = std::get_if<LWS::EventKeyDown>(&eventData);
        if (keyEvent == nullptr)
            return false;

        const LinuxKeyModifiers modifiers{
            .control = fPlatform.IsKeyPressed(LWS::KeyCode::Control).value_or(false),
            .shift   = fPlatform.IsKeyPressed(LWS::KeyCode::Shift).value_or(false),
            .alt     = fPlatform.IsKeyPressed(LWS::KeyCode::Alt).value_or(false),
            .win     = fPlatform.IsKeyPressed(LWS::KeyCode::Win).value_or(false),
        };
        bool handled        = false;
        const auto commands = fRawInputState->keyBindings.Resolve(keyEvent->key, modifiers);
        for (const std::string& command : commands)
            handled = ExecutePredefinedCommand(command) || handled;
        return handled;
    }

    std::intptr_t Viewer::ClientWindwMessage(const LWS::AnyEvent& eventData)
    {
        if (const auto* button = std::get_if<LWS::EventMouseButton>(&eventData))
        {
            fMouseInput->SetButton(button->button, button->pressed, true);
            return 1;
        }
        if (const auto* motion = std::get_if<LWS::EventMouseMove>(&eventData))
        {
            fMouseInput->Move(motion->delta);
            UpdateTexelPos();
            return 1;
        }
        if (const auto* wheel = std::get_if<LWS::EventMouseWheel>(&eventData))
        {
            fMouseInput->Wheel(wheel->steps(), fWindow.GetCanvasWindow().IsMouseInClientRect());
            return 1;
        }

        bool handled = false;
        if (std::holds_alternative<LWS::EventPaint>(eventData))
        {
            ResumePresentation();
            handled = true;
        }
        else if (std::holds_alternative<LWS::EventKeyDown>(eventData) ||
                 std::holds_alternative<LWS::EventKeyUp>(eventData))
        {
            handled = handleKeyInput(eventData);
        }
        return handled;
    }

    bool Viewer::HandleWinMessageEvent(const LWS::AnyEvent& eventData)
    {
        if (std::holds_alternative<LWS::EventKeyDown>(eventData) || std::holds_alternative<LWS::EventKeyUp>(eventData))
            return handleKeyInput(eventData);

        if (std::holds_alternative<LWS::EventFocusGained>(eventData))
            SetAppActive(true);
        else if (std::holds_alternative<LWS::EventFocusLost>(eventData))
        {
            fMouseInput->Cancel();
            SetAppActive(false);
        }
        else if (std::holds_alternative<LWS::EventPaint>(eventData) && !fIsFirstFrameDisplayed)
        {
            CompleteInitialization();
        }
        return false;
    }

}  // namespace OIV
