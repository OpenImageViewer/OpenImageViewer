#pragma once

#include <oivappcore/CommandManager.h>

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
        static std::wstring FormatSpeed(double speed);
        static uint32_t NextFrame(uint32_t currentFrame, uint32_t frameCount);
        static uint32_t FrameIntervalMs(uint32_t delayMilliseconds, double speed);
    };
}
