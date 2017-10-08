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
        bool showGrid;
    };

    enum RenderMode
    {
          RM_MainImage
        , RM_Overlay
    };

    struct ImageProperties
    {
        LLUtils::PointF64 position = LLUtils::PointF64::Zero;
        LLUtils::PointF64 scale = LLUtils::PointF64::One;
        RenderMode renderMode = RM_MainImage;
        double opacity = 1.0;
    };
}
