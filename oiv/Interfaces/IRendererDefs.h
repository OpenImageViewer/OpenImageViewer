#pragma once
#include <StringUtility.h>
#include <Point.h>

namespace OIV
{
    struct SelectionRect
    {
        LLUtils::PointI32 p0;
        LLUtils::PointI32 p1;
    };

    struct ViewParameters
    {
        LLUtils::PointF64 uvscale;
        LLUtils::PointF64 uvOffset;
        LLUtils::PointI32 uViewportSize;
        bool showGrid;
    };
}
