#include <oivappcore/InputGesturePolicy.h>

#include <cmath>

namespace OIV
{
    PanCursorHint InputGesturePolicy::CursorHintForPan(const LLUtils::PointF64& panAmount)
    {
        if (panAmount == LLUtils::PointF64::Zero)
            return {true, 0};

        constexpr double Pi = 3.14159265358979323846;
        constexpr int32_t NumDirections = 8;
        constexpr int32_t Step = 360 / NumDirections;
        const double degrees = std::atan2(-panAmount.y, panAmount.x) * 180.0 / Pi + 180.0;

        return {false, (static_cast<int32_t>(degrees) + Step / 2) % 360 / Step};
    }
}
