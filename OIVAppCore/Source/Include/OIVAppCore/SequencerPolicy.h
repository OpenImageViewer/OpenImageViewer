#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/CommandManager.h>

#include <cstdint>
#include <string>

namespace OIV
{
    class SequencerPolicy
    {
      public:

        static bool IsChangeSpeedCommand(const CommandManager::CommandArgs& args);
        static double ParseSpeedChangePercent(const CommandManager::CommandArgs& args);
        static double ApplySpeedChange(double currentSpeed, double percent);
        static LLUtils::native_string_type FormatSpeed(double speed);
        static uint32_t NextFrame(uint32_t currentFrame, uint32_t frameCount);
        static uint32_t FrameIntervalMs(uint32_t delayMilliseconds, double speed);
    };
}  // namespace OIV
