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
        LLUtils::PointI32 uViewportSize;
        LLUtils::Color uTransparencyColor1;
        LLUtils::Color uTransparencyColor2;
        bool showGrid;
    };
}
