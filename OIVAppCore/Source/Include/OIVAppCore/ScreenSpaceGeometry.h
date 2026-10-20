#pragma once

#include <LLUtils/Point.h>
#include <LLUtils/Rect.h>

#include <cstdint>
#include <optional>

namespace OIV::ScreenSpaceGeometry
{
    inline constexpr int MaxTextureDimension                = 16384;
    inline constexpr uint64_t DefaultPixelBufferTargetBytes = 64 * 1024 * 1024;
    inline constexpr double DefaultTexelThresholdPixels     = 5.0;

    // Image/fallback sizes are texels. Viewport and offset are physical screen pixels;
    // scale maps full-resolution rasterized texels to those screen pixels.
    struct View
    {
        LLUtils::Point<uint32_t> imageSize{};
        LLUtils::Point<uint32_t> fallbackSize{};
        LLUtils::PointI32 viewportSize{};
        LLUtils::PointF64 scale = LLUtils::PointF64::One;
        // The effective render offset, including any pan rounding already applied by the caller.
        LLUtils::PointF64 offset{};
    };

    // sourceRect uses full-resolution rasterized coordinates with an exclusive bottom/right edge.
    // textureSize is the output sample count, before GPU magnification by ImageState.
    struct Plan
    {
        LLUtils::RectI32 sourceRect{};
        LLUtils::PointI32 textureSize{};
    };

    // No pixel work or mutable state. Settings have already been validated at the configuration boundary.
    std::optional<Plan> BuildPlan(const View& view, double workFactor, uint64_t pixelBufferTargetBytes,
                                  double texelThresholdPixels);
    // cached describes an existing nonempty texture. Coverage includes the filtering border.
    bool CanReuse(const View& view, const Plan& cached, double zoomTolerance, double texelThresholdPixels);
}  // namespace OIV::ScreenSpaceGeometry
