#pragma once

#include <LLUtils/Point.h>
#include <LLUtils/Rect.h>

namespace OIV
{
    class ViewTransformController
    {
      public:
        static double FitScale(LLUtils::PointF64 viewportSize, LLUtils::PointF64 imageSize);
        static LLUtils::PointF64 CenterOffset(LLUtils::PointF64 viewportCenter, LLUtils::PointF64 visibleImageSize);
        static LLUtils::PointF64 ResolveOffset(LLUtils::PointF64 requestedOffset,
                                               LLUtils::PointF64 viewportSize,
                                               LLUtils::PointF64 visibleImageSize,
                                               LLUtils::PointF64 margins);
        static LLUtils::PointF64 ZoomOffset(LLUtils::PointF64 imageZoomPoint,
                                            double currentScale,
                                            double targetScale);
        static LLUtils::PointF64 ImageToClient(LLUtils::PointF64 imagePoint,
                                               double scale,
                                               LLUtils::PointF64 offset);
        static LLUtils::RectF64 ImageToClient(LLUtils::RectF64 imageRect,
                                              double scale,
                                              LLUtils::PointF64 offset);
        static LLUtils::PointF64 ClientToImage(LLUtils::PointF64 clientPoint,
                                               double scale,
                                               LLUtils::PointF64 offset);
        static LLUtils::RectF64 ClientToImage(LLUtils::RectF64 clientRect,
                                              double scale,
                                              LLUtils::PointF64 offset);

      private:
        static double ResolveAxisOffset(double viewportSize, double imageSize, double offset, double margin);
    };
}  // namespace OIV
