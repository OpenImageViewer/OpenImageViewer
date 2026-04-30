#include <oivappcore/FrameLimiterPolicy.h>

namespace OIV
{
    int64_t FrameLimiterPolicy::FrameWindowMicroseconds(int64_t refreshRateTimes1000)
    {
        return 1'000'000'000 / refreshRateTimes1000;
    }

    FrameRefreshDecision FrameLimiterPolicy::Decide(bool enabled,
                                                    bool timerEnabled,
                                                    int64_t microsecondsSinceLastRefresh,
                                                    int64_t refreshRateTimes1000)
    {
        if (enabled == false)
            return {FrameRefreshAction::RefreshNow};

        const int64_t windowTimeInMicroseconds = FrameWindowMicroseconds(refreshRateTimes1000);
        if (microsecondsSinceLastRefresh > windowTimeInMicroseconds)
            return {FrameRefreshAction::RefreshNow};

        if (timerEnabled == false)
            return {FrameRefreshAction::ScheduleRefresh,
                    (windowTimeInMicroseconds - microsecondsSinceLastRefresh) / 1000};

        return {};
    }
}  // namespace OIV
