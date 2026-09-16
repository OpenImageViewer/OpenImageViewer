#include <OIVAppCore/ViewActionController.h>

#include <algorithm>

namespace OIV
{
    double ViewActionController::MinimumPixelSize(double minimumImageSize, LLUtils::PointF64 transformedImageSize)
    {
        if (transformedImageSize.x <= 0.0 || transformedImageSize.y <= 0.0)
            return 1.0;

        const LLUtils::PointF64 minimumZoom = minimumImageSize / transformedImageSize;
        return std::min(std::max(minimumZoom.x, minimumZoom.y), 1.0);
    }

    double ViewActionController::ResolveZoomValue(double requestedZoom,
                                                  bool fitToScreenLocked,
                                                  double minimumZoom,
                                                  double maximumZoom)
    {
        if (fitToScreenLocked)
            return requestedZoom;

        return std::clamp(requestedZoom, minimumZoom, maximumZoom);
    }

    LLUtils::PointI32 ViewActionController::ResolveZoomPoint(LLUtils::PointI32 requestedPoint,
                                                             LLUtils::PointF64 canvasCenter)
    {
        LLUtils::PointI32 resolvedPoint = requestedPoint;
        const LLUtils::PointI32 center = static_cast<LLUtils::PointI32>(canvasCenter);

        if (resolvedPoint.x < 0)
            resolvedPoint.x = center.x;
        if (resolvedPoint.y < 0)
            resolvedPoint.y = center.y;

        return resolvedPoint;
    }

    double ViewActionController::RelativeZoom(double currentScale, double adaptiveAmount)
    {
        constexpr double OriginalScale = 1.0;
        constexpr double SnapTolerance = 0.03;
        double predictedScale          = adaptiveAmount > 0 ? currentScale * (1 + adaptiveAmount)
                                                            : currentScale / (1 - adaptiveAmount);

        // Directional bounds include both entering the snap range and crossing beyond it.
        // Strict movement checks let the next event leave 100% and keep zero movement unchanged.
        const bool snapFromBelow = currentScale < OriginalScale && predictedScale > currentScale &&
                                   predictedScale >= OriginalScale - SnapTolerance;
        const bool snapFromAbove = currentScale > OriginalScale && predictedScale < currentScale &&
                                   predictedScale <= OriginalScale + SnapTolerance;
        if (snapFromBelow || snapFromAbove)
            predictedScale = OriginalScale;

        return predictedScale;
    }

    bool ViewActionController::ShouldPreserveOffsetLockForZoom(int32_t clientX, int32_t clientY)
    {
        return clientX == -1 && clientY == -1;
    }

    bool ViewActionController::ShouldFitToScreenOnAutoPlace(bool fitToScreenLocked, bool offsetLocked)
    {
        return fitToScreenLocked && offsetLocked;
    }

    bool ViewActionController::ShouldCenterOnAutoPlace(bool fitToScreenLocked, bool offsetLocked, bool forceCenter)
    {
        return (fitToScreenLocked == false && offsetLocked) || forceCenter;
    }
}
