#pragma once

#include <LLUtils/Point.h>

#include <cstdint>

namespace OIV
{
    struct PanCursorHint
    {
        bool sizeAll = false;
        int32_t directionIndex = 0;
    };

    class InputGesturePolicy
    {
      public:
        static PanCursorHint CursorHintForPan(const LLUtils::PointF64& panAmount);
    };
}
