#include <OIVShared/ViewTransformController.h>

#include <algorithm>

namespace OIV
{
    double ViewTransformController::FitScale(LLUtils::PointF64 viewportSize, LLUtils::PointF64 imageSize)
    {
        if (viewportSize.x <= 0.0 || viewportSize.y <= 0.0 || imageSize.x <= 0.0 || imageSize.y <= 0.0)
            return 1.0;

        const LLUtils::PointF64 ratio = viewportSize / imageSize;
        return std::min(ratio.x, ratio.y);
    }

    LLUtils::PointF64 ViewTransformController::CenterOffset(LLUtils::PointF64 viewportCenter,
                                                            LLUtils::PointF64 visibleImageSize)
    {
        return viewportCenter - visibleImageSize / 2.0;
    }

    LLUtils::PointF64 ViewTransformController::ResolveOffset(LLUtils::PointF64 requestedOffset,
                                                             LLUtils::PointF64 viewportSize,
                                                             LLUtils::PointF64 visibleImageSize,
                                                             LLUtils::PointF64 margins)
    {
        return {ResolveAxisOffset(viewportSize.x, visibleImageSize.x, requestedOffset.x, margins.x),
                ResolveAxisOffset(viewportSize.y, visibleImageSize.y, requestedOffset.y, margins.y)};
    }

    LLUtils::PointF64 ViewTransformController::ZoomOffset(LLUtils::PointF64 imageZoomPoint,
                                                          double currentScale,
                                                          double targetScale)
    {
        return imageZoomPoint * (currentScale - targetScale);
    }

    LLUtils::PointF64 ViewTransformController::ImageToClient(LLUtils::PointF64 imagePoint,
                                                             double scale,
                                                             LLUtils::PointF64 offset)
    {
        return imagePoint * scale + offset;
    }

    LLUtils::RectF64 ViewTransformController::ImageToClient(LLUtils::RectF64 imageRect,
                                                            double scale,
                                                            LLUtils::PointF64 offset)
    {
        return {ImageToClient(imageRect.GetCorner(LLUtils::Corner::TopLeft), scale, offset),
                ImageToClient(imageRect.GetCorner(LLUtils::Corner::BottomRight), scale, offset)};
    }

    LLUtils::PointF64 ViewTransformController::ClientToImage(LLUtils::PointF64 clientPoint,
                                                             double scale,
                                                             LLUtils::PointF64 offset)
    {
        return (clientPoint - offset) / scale;
    }

    LLUtils::RectF64 ViewTransformController::ClientToImage(LLUtils::RectF64 clientRect,
                                                            double scale,
                                                            LLUtils::PointF64 offset)
    {
        return {ClientToImage(clientRect.GetCorner(LLUtils::Corner::TopLeft), scale, offset),
                ClientToImage(clientRect.GetCorner(LLUtils::Corner::BottomRight), scale, offset)};
    }

    double ViewTransformController::ResolveAxisOffset(double viewportSize,
                                                      double imageSize,
                                                      double offset,
                                                      double margin)
    {
        if (imageSize > viewportSize)
        {
            if (offset > 0.0)
                return std::min(viewportSize * margin, offset);

            if (offset < 0.0)
                return std::max(-imageSize + viewportSize * (1.0 - margin), offset);
        }
        else
        {
            if (offset < 0.0)
                return std::max(-imageSize * margin, offset);

            if (offset > 0.0)
                return std::min(viewportSize - imageSize * (1.0 - margin), offset);
        }

        return offset;
    }
}  // namespace OIV
