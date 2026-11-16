#pragma once
#include <Image.h>
#include <LLUtils/Color.h>
#include <LLUtils/Point.h>
#include <LLUtils/Rect.h>
#include <array>
#include <expected>
#include <string>
#include <string_view>

namespace OIV
{
    enum class ResizeAspectRatio
    {
        Preserve,
        DoNotPreserve,
        BlackBars
    };
    enum class ResizeFilter
    {
        Box = 1,
        Triangle,
        CubicBSpline,
        CatmullRom,
        Mitchell,
        Point,
        MitchellQuarter,
        MitchellSixth,
        MitchellNegativeSixth,
        MitchellNegativeThird,
        Lanczos4,
        Lanczos5,
        Muratori6
    };
    inline constexpr std::array ResizeFilters{ResizeFilter::Point,
                                              ResizeFilter::Box,
                                              ResizeFilter::Triangle,
                                              ResizeFilter::CubicBSpline,
                                              ResizeFilter::CatmullRom,
                                              ResizeFilter::Mitchell,
                                              ResizeFilter::MitchellQuarter,
                                              ResizeFilter::MitchellSixth,
                                              ResizeFilter::MitchellNegativeSixth,
                                              ResizeFilter::MitchellNegativeThird,
                                              ResizeFilter::Lanczos4,
                                              ResizeFilter::Lanczos5,
                                              ResizeFilter::Muratori6};
    std::string_view ResizeFilterName(ResizeFilter filter);
    // General-purpose recommendations, not content classification. Resolve each axis independently.
    constexpr ResizeFilter AutoResizeFilter(int32_t input, int32_t output)
    {
        return output == input                         ? ResizeFilter::Point
               : output < input                        ? ResizeFilter::Mitchell
               : int64_t(output) <= int64_t(input) * 2 ? ResizeFilter::CatmullRom
                                                       : ResizeFilter::Mitchell;
    }
    constexpr ResizeAspectRatio PresetResizeAspect(ResizeAspectRatio aspect, LLUtils::PointI32 source,
                                                   LLUtils::PointI32 target)
    {
        return aspect == ResizeAspectRatio::Preserve && int64_t(source.x) * target.y != int64_t(source.y) * target.x
                   ? ResizeAspectRatio::BlackBars
                   : aspect;
    }
    struct ImageResizeRequest
    {
        LLUtils::PointI32 size;
        ResizeAspectRatio aspect = ResizeAspectRatio::Preserve;
        double dpi               = 96;
        bool autoFilter          = true;
        ResizeFilter filter      = ResizeFilter::Mitchell;
        // Uniform resizing retains this axis and derives the other before rounding.
        bool preserveHeight = false;
        // RGBA in the source color space; ignored unless letterboxing adds padding.
        LLUtils::Color letterboxColor{0, 0, 0};
    };
    struct ImageResizeLayout
    {
        LLUtils::PointI32 canvas;
        LLUtils::PointI32 content;
        LLUtils::PointI32 offset;
        ResizeFilter horizontal;
        ResizeFilter vertical;
    };
    std::expected<ImageResizeLayout, std::string> ResolveImageResize(LLUtils::PointI32 source,
                                                                     const ImageResizeRequest& request);
    // Uses full-precision, already oriented pixels. Crop coordinates refer to that image, not its display preview.
    // The source is immutable; sample precision and color interpretation are retained.
    // Transparent padding promotes integer RGB/grayscale to RGBA; float formats without alpha are rejected.
    std::expected<IMCodec::ImageSharedPtr, std::string> ResizeImage(const IMCodec::ImageSharedPtr& source,
                                                                    LLUtils::RectI32 crop,
                                                                    const ImageResizeRequest& request);
}  // namespace OIV
