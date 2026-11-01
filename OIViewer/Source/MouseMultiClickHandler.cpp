#include "MouseMultiClickHandler.h"

#include <limits>

namespace OIV
{
    MouseMultiClickHandler::MouseMultiClickHandler(LWS::PlatformContext& platform, uint16_t multipressRate,
                                                   uint16_t maxTaps)
        : fMultiPressThreshold(multipressRate), fMaxTaps(maxTaps),
          fTimer(platform, std::bind(&MouseMultiClickHandler::TimerCallback, this))
    {
        fTimer.SetDueTime(multipressRate);
        fTimer.SetRepeatInterval(std::numeric_limits<uint32_t>::max());
    }

    MouseMultiClickHandler::ButtonData& MouseMultiClickHandler::GetButtonData(LWS::MouseButton buttonId)
    {
        return fButtonsData[static_cast<size_t>(buttonId)];
    }

    void MouseMultiClickHandler::SetMouseDelta(int16_t deltaX, int16_t deltaY)
    {
        fPosX += deltaX;
        fPosY += deltaY;
    }

    void MouseMultiClickHandler::Reset()
    {
        fTimer.Enable(false);
        fPressedButtons.clear();
        fButtonsData = {};
        fPosX        = 0;
        fPosY        = 0;
    }

    void MouseMultiClickHandler::ProcessQueuedButtons()
    {
        std::set<LWS::MouseButton> buttonsRemoved;

        int64_t minTimeToEvent = (std::numeric_limits<int64_t>::min)();
        for (auto& button : fPressedButtons)
        {
            auto& buttonData = GetButtonData(button);
            uint64_t now = static_cast<uint64_t>(fStopWatch.GetElapsedTimeInteger(LLUtils::StopWatch::Milliseconds));

            int64_t timeSinceActuation = static_cast<int64_t>(now) -
                                         static_cast<int64_t>(buttonData.timestampLastButtonDown);

            int64_t timeToEvent = timeSinceActuation - fMultiPressThreshold;
            if (timeToEvent >= 0)
            {
                OnMouseClickEvent.Raise(EventArgs{button, buttonData.tapCounter});
                buttonData.tapCounter = 0;
                buttonsRemoved.insert(button);
            }
            else
            {
                // timeToEvent is negative, get the closest number to zero.
                minTimeToEvent = (std::max) (minTimeToEvent, timeToEvent);
            }
        }

        if (minTimeToEvent != (std::numeric_limits<int64_t>::min)())
        {
            fTimer.SetDueTime(static_cast<uint32_t>(-minTimeToEvent));
            fTimer.Enable(true);
        }

        for (const auto& remove : buttonsRemoved)
            fPressedButtons.erase(remove);

        ResetPosition();
    }

    void MouseMultiClickHandler::ResetPosition()
    {
        if (fPressedButtons.empty() == true)
        {
            fPosX = 0;
            fPosY = 0;
            fTimer.Enable(false);
        }
    }

    void MouseMultiClickHandler::TimerCallback()
    {
        ProcessQueuedButtons();
    }
    void MouseMultiClickHandler::RemoveButton(LWS::MouseButton button)
    {
        fPressedButtons.erase(button);
        ResetPosition();
        GetButtonData(button).tapCounter = 0;
    }

    // Get the state of a button whether it's down or up
    void MouseMultiClickHandler::SetButtonState(LWS::MouseButton button, bool pressed)
    {
        ButtonData& buttonData          = GetButtonData(button);
        const uint64_t currentTimeStamp = static_cast<uint64_t>(
            fStopWatch.GetElapsedTimeInteger(LLUtils::StopWatch::Milliseconds));

        if (buttonData.pressed != pressed)
        {
            if (pressed)
            {
                buttonData.tapCounter++;
                buttonData.timestampLastButtonDown = currentTimeStamp;

                if (buttonData.tapCounter == 1)
                {
                    buttonData.posX = fPosX;
                    buttonData.posY = fPosY;
                    fPressedButtons.insert(button);

                    if (fTimer.GetEnabled() == false)
                    {
                        fTimer.SetDueTime(fMultiPressThreshold);
                        fTimer.Enable(true);
                    }
                }

                else
                {
                    const int radiusSquard = (buttonData.posX - fPosX) * (buttonData.posX - fPosX) +
                                             (buttonData.posY - fPosY) * (buttonData.posY - fPosY);
                    if (radiusSquard < fMaxMultiClickRadius * fMaxMultiClickRadius)
                    {
                        if (buttonData.tapCounter == fMaxTaps)
                        {
                            OnMouseClickEvent.Raise(EventArgs{button, buttonData.tapCounter});
                            RemoveButton(button);
                        }
                    }
                    else
                    {
                        buttonData.posX       = fPosX;
                        buttonData.posY       = fPosY;
                        buttonData.tapCounter = 1;
                    }
                }
            }
            buttonData.pressed = pressed;
        }
    }
}  // namespace OIV
