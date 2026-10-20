#include <OIVAppCore/ScreenSpaceGeometry.h>

#include <algorithm>
#include <cmath>

namespace OIV::ScreenSpaceGeometry
{
    namespace
    {
        // Crop an already active view. Margin planning can reuse this without repeating activation checks.
        LLUtils::RectI32 SourceRegion(const View& view, double margin)
        {
            LLUtils::RectI32 region{};
            const auto imageSize = static_cast<LLUtils::PointF64>(view.imageSize);
            const auto padding   = static_cast<LLUtils::PointF64>(view.viewportSize) * margin;
            const LLUtils::PointF64 first{std::max(0.0, (-view.offset.x - padding.x) / view.scale.x),
                                          std::max(0.0, (-view.offset.y - padding.y) / view.scale.y)};
            const LLUtils::PointF64 last{
                std::min(imageSize.x, (view.viewportSize.x + padding.x - view.offset.x) / view.scale.x),
                std::min(imageSize.y, (view.viewportSize.y + padding.y - view.offset.y) / view.scale.y)};
            // Rect normalizes endpoints: reject empty intersections before constructing the crop.
            if (first.x < last.x && first.y < last.y)
            {
                // Round outward and retain one source texel for filtering at the visible edges.
                region = {{static_cast<int32_t>(std::max(0.0, std::floor(first.x) - 1.0)),
                           static_cast<int32_t>(std::max(0.0, std::floor(first.y) - 1.0))},
                          {static_cast<int32_t>(std::min(imageSize.x, std::ceil(last.x) + 1.0)),
                           static_cast<int32_t>(std::min(imageSize.y, std::ceil(last.y) + 1.0))}};
            }
            return region;
        }

        LLUtils::RectI32 VisibleRegion(const View& view, double texelThresholdPixels)
        {
            LLUtils::RectI32 region{};
            if (view.imageSize != view.fallbackSize && view.fallbackSize.x > 0 && view.fallbackSize.y > 0 &&
                view.viewportSize.x > 0 && view.viewportSize.y > 0 && view.scale.x > 0.0 && view.scale.y > 0.0)
            {
                const auto proxyScale = view.scale * static_cast<LLUtils::PointF64>(view.imageSize) /
                                        static_cast<LLUtils::PointF64>(view.fallbackSize);
                if (std::max(proxyScale.x, proxyScale.y) >= texelThresholdPixels)
                    region = SourceRegion(view, 0.0);
            }
            return region;
        }

        LLUtils::PointI32 TargetSize(const LLUtils::RectI32& region, LLUtils::PointF64 scale)
        {
            // Desired dimensions before GPU limits: native texels are copied, never upsampled on the CPU.
            return {static_cast<int32_t>(std::max(1.0, std::round(region.GetWidth() * std::min(scale.x, 1.0)))),
                    static_cast<int32_t>(std::max(1.0, std::round(region.GetHeight() * std::min(scale.y, 1.0))))};
        }

        LLUtils::RectI32 FitMargins(const View& view, const LLUtils::RectI32& visible, double margin,
                                    uint64_t pixelBufferTargetBytes)
        {
            const auto fits = [&](const LLUtils::RectI32& region)
            {
                const auto target = TargetSize(region, view.scale);
                return target.x <= MaxTextureDimension && target.y <= MaxTextureDimension &&
                       static_cast<uint64_t>(target.x) * target.y * sizeof(uint32_t) <= pixelBufferTargetBytes;
            };
            auto region = visible;
            // Resource limits trim optional coverage only. The viewport and its filtering border remain necessary.
            if (margin > 0.0 && fits(visible))
            {
                const auto expanded = SourceRegion(view, margin);
                if (fits(expanded))
                    region = expanded;
                else
                {
                    // Bound planning cost with a fixed search for a usable margin near the resource limit.
                    double lower              = 0.0;
                    double upper              = margin;
                    constexpr int SearchSteps = 16;
                    for (int step = 0; step < SearchSteps; ++step)
                    {
                        const double candidateMargin = (lower + upper) / 2.0;
                        const auto candidate         = SourceRegion(view, candidateMargin);
                        if (fits(candidate))
                        {
                            lower  = candidateMargin;
                            region = candidate;
                        }
                        else
                            upper = candidateMargin;
                    }
                }
            }
            return region;
        }

        bool ResolutionMatches(const View& view, const Plan& cached, const LLUtils::RectI32& required, double tolerance)
        {
            const LLUtils::PointF64 sourceSize{static_cast<double>(cached.sourceRect.GetWidth()),
                                               static_cast<double>(cached.sourceRect.GetHeight())};
            // Compare rounded sample counts. A 25% tolerance gives [0.8, 1.25], including exact reuse at zero
            // tolerance. The GPU cap still bounds attainable resolution on unusually large framebuffers.
            const LLUtils::PointF64 rate{
                std::min({view.scale.x, 1.0, static_cast<double>(MaxTextureDimension) / required.GetWidth()}),
                std::min({view.scale.y, 1.0, static_cast<double>(MaxTextureDimension) / required.GetHeight()})};
            const auto wanted = (sourceSize * rate).Round();
            const LLUtils::PointF64 ratio{std::max(1.0, wanted.x) / cached.textureSize.x,
                                          std::max(1.0, wanted.y) / cached.textureSize.y};
            const double upper = 1.0 + tolerance;
            const double lower = 1.0 / upper;
            // Tolerance cannot conceal missing original texels at native zoom when those texels fit the GPU.
            return ratio.x >= lower && ratio.x <= upper && ratio.y >= lower && ratio.y <= upper &&
                   (view.scale.x < 1.0 || required.GetWidth() > MaxTextureDimension ||
                    cached.textureSize.x == sourceSize.x) &&
                   (view.scale.y < 1.0 || required.GetHeight() > MaxTextureDimension ||
                    cached.textureSize.y == sourceSize.y);
        }
    }  // namespace

    std::optional<Plan> BuildPlan(const View& view, double workFactor, uint64_t pixelBufferTargetBytes,
                                  double texelThresholdPixels)
    {
        std::optional<Plan> plan;
        const auto visible = VisibleRegion(view, texelThresholdPixels);
        if (!visible.IsEmpty())
        {
            const auto imageSize = static_cast<LLUtils::PointF64>(view.imageSize);
            const auto viewport  = static_cast<LLUtils::PointF64>(view.viewportSize);
            // Work is source texels, not milliseconds. Physical viewport pixels set the budget.
            // S = min(imageWidth, viewportWidth / scaleX) * min(imageHeight, viewportHeight / scaleY)
            // B = viewportWidth * viewportHeight * workFactor
            // A margin m per side grows area by (1 + 2m)^2, hence m = (sqrt(B / S) - 1) / 2.
            // The full-viewport estimate is conservative when only part of the image overlaps the viewport.
            const double sourcePixels = std::min(imageSize.x, viewport.x / view.scale.x) *
                                        std::min(imageSize.y, viewport.y / view.scale.y);
            const double budget       = viewport.x * viewport.y * workFactor;
            const double margin       = std::clamp((std::sqrt(budget / sourcePixels) - 1.0) / 2.0, 0.0, 0.5);
            const auto region         = FitMargins(view, visible, margin, pixelBufferTargetBytes);
            const auto target         = TargetSize(region, view.scale);
            plan                      = Plan{.sourceRect  = region,
                                             .textureSize = {std::min(target.x, MaxTextureDimension),
                                                             std::min(target.y, MaxTextureDimension)}};
        }
        return plan;
    }

    bool CanReuse(const View& view, const Plan& cached, double zoomTolerance, double texelThresholdPixels)
    {
        const auto required = VisibleRegion(view, texelThresholdPixels);
        return !required.IsEmpty() && required.IsInside(cached.sourceRect) &&
               ResolutionMatches(view, cached, required, zoomTolerance);
    }
}  // namespace OIV::ScreenSpaceGeometry
