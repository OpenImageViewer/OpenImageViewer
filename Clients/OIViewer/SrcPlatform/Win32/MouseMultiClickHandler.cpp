#include "MouseMultiClickHandler.h"
namespace OIV
{
	MouseMultiClickHandler::MouseMultiClickHandler(uint16_t multipressRate, uint16_t maxTaps) :

		fMultiPressThreshold(multipressRate)
		, fMaxTaps(maxTaps)
	{
		fTimer.SetDueTime(multipressRate);
		fTimer.SetRepeatInterval(INFINITE);

	}

	MouseMultiClickHandler::ButtonData& MouseMultiClickHandler::GetButtonData(LInput::MouseButton buttonId)
	{
		return fButtonsData[static_cast<size_t>(buttonId)];
	}

	void MouseMultiClickHandler::SetMouseDelta(int16_t deltaX, int16_t deltaY)
	{
		fPosX += deltaX;
		fPosY += deltaY;
	}

	void MouseMultiClickHandler::ProcessQueuedButtons()
	{
		std::set<LInput::MouseButton> buttonsRemoved;

		int64_t minTimeToEvent = (std::numeric_limits<int64_t>::min)();
		for (auto& button : fPressedButtons)
		{
			auto& buttonData = GetButtonData(button);
			uint64_t now = static_cast<uint64_t>(fStopWatch.GetElapsedTimeInteger(LLUtils::StopWatch::Milliseconds));

			int64_t timeSinceActuation = static_cast<int64_t>(now) - static_cast<int64_t>(buttonData.timestampLastButtonDown);

			int64_t timeToEvent = timeSinceActuation - fMultiPressThreshold;
			if (timeToEvent >= 0)
			{
				OnMouseClickEvent.Raise(EventArgs{ button, buttonData.tapCounter });
				buttonData.tapCounter = 0;
				buttonsRemoved.insert(button);

			}
			else
			{
				// timeToEvent is negative, get the closest number to zero.
				minTimeToEvent = (std::max)(minTimeToEvent, timeToEvent);
			}
		}

		if (minTimeToEvent != (std::numeric_limits<int64_t>::min)())
		{
			fTimer.SetDueTime(static_cast<DWORD>(-minTimeToEvent));
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
	void MouseMultiClickHandler::RemoveButton(LInput::MouseButton button)
	{
		fPressedButtons.erase(button);
		ResetPosition();
		GetButtonData(button).tapCounter = 0;
	}

	void MouseMultiClickHandler::StartTimer()
	{


	}

	// Get the state of a button whether it's down or up
	void MouseMultiClickHandler::SetButtonState(LInput::MouseButton button, LInput::ButtonState newState)
	{
		if (newState != LInput::ButtonState::NotSet)
		{
			ButtonData& buttonData = GetButtonData(button);
			const uint64_t currentTimeStamp = static_cast<uint64_t>(fStopWatch.GetElapsedTimeInteger(LLUtils::StopWatch::Milliseconds));

			if (buttonData.buttonState != newState)
			{
				if (newState == LInput::ButtonState::Down)
				{
					if (buttonData.buttonState == LInput::ButtonState::Up)
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
							
							const int radiusSquard = (buttonData.posX - fPosX) * (buttonData.posX - fPosX) + (buttonData.posY - fPosY) * (buttonData.posY - fPosY);
							const bool inRadius = radiusSquard < fMaxMultiClickRadius* fMaxMultiClickRadius;

							if (inRadius == true)
							{
								
								if (buttonData.tapCounter == fMaxTaps) // reached max taps, raise an event
								{
									
									OnMouseClickEvent.Raise(EventArgs{ button, buttonData.tapCounter });
									RemoveButton(button);
								}
							}
							else
							{
								//if click is out of radius, set this event as initial click								buttonData.posX = fPosX;
								buttonData.posX = fPosX;
								buttonData.posY = fPosY;
								buttonData.tapCounter = 1;
							}
						}
					
					}
				}
				buttonData.buttonState = newState;
			}
		}
	}
}