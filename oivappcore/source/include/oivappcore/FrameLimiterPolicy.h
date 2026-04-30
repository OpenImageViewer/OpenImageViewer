#pragma once

#include <cstdint>

namespace OIV
{
    enum class FrameRefreshAction
    {
        None,
        RefreshNow,
        ScheduleRefresh
    };

    struct FrameRefreshDecision
    {
        FrameRefreshAction action = FrameRefreshAction::None;
        int64_t delayMs = 0;
    };

    class FrameLimiterPolicy
    {
      public:
        static int64_t FrameWindowMicroseconds(int64_t refreshRateTimes1000);
        static FrameRefreshDecision Decide(bool enabled,
                                           bool timerEnabled,
                                           int64_t microsecondsSinceLastRefresh,
                                           int64_t refreshRateTimes1000);
    };
}  // namespace OIV
