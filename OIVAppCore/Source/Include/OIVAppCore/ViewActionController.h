#pragma once

#include <LLUtils/Point.h>

#include <cstdint>

namespace OIV
{
    class ViewActionController
    {
      public:
        static double MinimumPixelSize(double minimumImageSize, LLUtils::PointF64 transformedImageSize);
        static double ResolveZoomValue(double requestedZoom,
                                       bool fitToScreenLocked,
                                       double minimumZoom,
                                       double maximumZoom);
        static LLUtils::PointI32 ResolveZoomPoint(LLUtils::PointI32 requestedPoint, LLUtils::PointF64 canvasCenter);
        static double RelativeZoom(double currentScale, double adaptiveAmount);
        static bool ShouldPreserveOffsetLockForZoom(int32_t clientX, int32_t clientY);
        static bool ShouldFitToScreenOnAutoPlace(bool fitToScreenLocked, bool offsetLocked);
        static bool ShouldCenterOnAutoPlace(bool fitToScreenLocked, bool offsetLocked, bool forceCenter);
    };
}
