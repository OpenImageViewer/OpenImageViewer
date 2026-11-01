/*
Copyright (c) 2021 Lior Lahav

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <vector>
#include <set>
#include <LLUtils/StopWatch.h>
#include <LLUtils/Event.h>
#include <LWS/Timer.hpp>
#include <LWS/MouseButton.hpp>

namespace OIV
{

    class MouseMultiClickHandler final
    {
      public:

        struct EventArgs
        {
            LWS::MouseButton button;
            uint16_t clickCount;
        };

        MouseMultiClickHandler(LWS::PlatformContext& platform, uint16_t multipressRate, uint16_t maxTaps);

        LLUtils::Event<void(const EventArgs&)> OnMouseClickEvent;
        using ListButtonEvent = std::vector<EventArgs>;

      private:

        struct ButtonData
        {
            uint64_t timestampLastButtonDown = 0;
            uint16_t tapCounter              = 0;
            int16_t posX;
            int16_t posY;
            bool pressed = false;
        };

      public:

        // Get the state of a button whether it's down or up
        void SetButtonState(LWS::MouseButton button, bool pressed);
        void SetMouseDelta(int16_t deltaX, int16_t deltaY);
        void Reset();

      private:

        ButtonData& GetButtonData(LWS::MouseButton buttonId);
        void ProcessQueuedButtons();
        void ResetPosition();
        void TimerCallback();
        void RemoveButton(LWS::MouseButton button);

      private:

        /// <summary>
        /// time in millisecond between actuation that determines the time threshold between presses
        /// </summary>
        uint16_t fMultiPressThreshold = 250;
        /// <summary>
        /// the repeat rate in milliseconds, set to zero (0) to disable repeat rate
        /// </summary>
        uint16_t fMaxTaps            = 3;
        int16_t fPosX                = 0;
        int16_t fPosY                = 0;
        int16_t fMaxMultiClickRadius = 10;
        LWS::HighPrecisionTimer fTimer;

        std::array<ButtonData, static_cast<size_t>(LWS::MouseButton::Count)> fButtonsData{};

        /// <summary>
        /// used for sending key repeaet signals to the client
        /// </summary>
        std::set<LWS::MouseButton> fPressedButtons;
        LLUtils::StopWatch fStopWatch = LLUtils::StopWatch(true);
    };
}  // namespace OIV
