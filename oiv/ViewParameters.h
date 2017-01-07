#pragma once
#include "Point.h"
namespace OIV
{
    class ViewParameters
    {
    public:
        LLUtils::PointF64 uvscale;
        LLUtils::PointF64 uvOffset;
        LLUtils::PointI32 uViewportSize;
        bool showGrid;
    };
}
