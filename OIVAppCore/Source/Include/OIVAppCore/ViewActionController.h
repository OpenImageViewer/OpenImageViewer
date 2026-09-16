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
        // Snap approaching zoom to 100% when its accelerated target enters 97-103% or crosses 100%.
        // Starting at 100% or moving away bypasses snapping; this does not reset acceleration.
        static double RelativeZoom(double currentScale, double adaptiveAmount);
        static bool ShouldPreserveOffsetLockForZoom(int32_t clientX, int32_t clientY);
        static bool ShouldFitToScreenOnAutoPlace(bool fitToScreenLocked, bool offsetLocked);
        static bool ShouldCenterOnAutoPlace(bool fitToScreenLocked, bool offsetLocked, bool forceCenter);
    };
}
