#include <ImageProcessing/ImageResize.h>
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize2.h>
#include <algorithm>
#include <ExoticNumbers/half.hpp>
#include <cmath>
#include <cstring>
#include <limits>

namespace OIV
{
    static_assert(int(ResizeFilter::Muratori6) == STBIR_FILTER_MURATORI6);
    static_assert(int(ResizeFilter::Point) == STBIR_FILTER_POINT_SAMPLE);
    std::string_view ResizeFilterName(ResizeFilter filter)
    {
        switch (filter)
        {
            case ResizeFilter::Point:
                return "Point / nearest neighbor";
            case ResizeFilter::Box:
                return "Box";
            case ResizeFilter::Triangle:
                return "Triangle / bilinear";
            case ResizeFilter::CubicBSpline:
                return "Cubic B-spline";
            case ResizeFilter::CatmullRom:
                return "Catmull–Rom";
            case ResizeFilter::Mitchell:
                return "Mitchell";
            case ResizeFilter::MitchellQuarter:
                return "Mitchell quarter / Robidoux";
            case ResizeFilter::MitchellSixth:
                return "Mitchell sixth / sharper";
            case ResizeFilter::MitchellNegativeSixth:
                return "Mitchell negative sixth / strong sharpen";
            case ResizeFilter::MitchellNegativeThird:
                return "Mitchell negative third / strongest sharpen";
            case ResizeFilter::Lanczos4:
                return "Lanczos 4-tap / 2 lobes";
            case ResizeFilter::Lanczos5:
                return "Lanczos 5-tap / 2.5 lobes";
            case ResizeFilter::Muratori6:
                return "Muratori 6-tap";
        }
        return "Unknown";
    }
    std::expected<ImageResizeLayout, std::string> ResolveImageResize(LLUtils::PointI32 source,
                                                                     const ImageResizeRequest& request)
    {
        if (source.x <= 0 || source.y <= 0 || request.size.x <= 0 || request.size.y <= 0 ||
            !std::isfinite(request.dpi) || request.dpi <= 0 ||
            (request.aspect != ResizeAspectRatio::Preserve && request.aspect != ResizeAspectRatio::DoNotPreserve &&
             request.aspect != ResizeAspectRatio::BlackBars) ||
            std::ranges::find(ResizeFilters, request.filter) == ResizeFilters.end())
            return std::unexpected("Invalid resize dimensions, DPI, or filter");
        ImageResizeLayout layout{.canvas = request.size, .content = request.size};
        if (request.aspect == ResizeAspectRatio::Preserve)
        {
            const double dependent = request.preserveHeight ? std::round(double(request.size.y) * source.x / source.y)
                                                            : std::round(double(request.size.x) * source.y / source.x);
            if (dependent > std::numeric_limits<int32_t>::max())
                return std::unexpected("Output dimensions are too large");
            auto& canvas  = request.preserveHeight ? layout.canvas.x : layout.canvas.y;
            auto& content = request.preserveHeight ? layout.content.x : layout.content.y;
            canvas = content = std::max(1, static_cast<int32_t>(dependent));
        }
        else if (request.aspect == ResizeAspectRatio::BlackBars)
        {
            const double scale = std::min(double(request.size.x) / source.x, double(request.size.y) / source.y);
            layout.content     = {std::clamp(static_cast<int32_t>(std::round(source.x * scale)), 1, request.size.x),
                                  std::clamp(static_cast<int32_t>(std::round(source.y * scale)), 1, request.size.y)};
            layout.offset      = {(layout.canvas.x - layout.content.x) / 2, (layout.canvas.y - layout.content.y) / 2};
        }
        layout.horizontal = request.autoFilter ? AutoResizeFilter(source.x, layout.content.x) : request.filter;
        layout.vertical   = request.autoFilter ? AutoResizeFilter(source.y, layout.content.y) : request.filter;
        return layout;
    }
    namespace
    {
        struct PixelLayout
        {
            stbir_pixel_layout layout;
            stbir_datatype type;
            int channelBytes;
        };
        std::expected<PixelLayout, std::string> GetPixelLayout(const IMCodec::Image& image)
        {
            using namespace IMCodec;
            const auto& info = image.GetTexelInfo();
            const auto first = info.channles[0];
            if (!info.numChannles || info.numChannles > 4)
                return std::unexpected("Unsupported image channels");
            PixelLayout result{.layout = STBIR_1CHANNEL, .type = STBIR_TYPE_UINT8, .channelBytes = first.width / 8};
            if (first.channelDataType == ChannelDataType::UnsignedInt && first.width == 8)
                result.type = STBIR_TYPE_UINT8;
            else if (first.channelDataType == ChannelDataType::UnsignedInt && first.width == 16)
                result.type = STBIR_TYPE_UINT16;
            else if (first.channelDataType == ChannelDataType::Float && first.width == 16)
                result.type = STBIR_TYPE_HALF_FLOAT;
            else if (first.channelDataType == ChannelDataType::Float && first.width == 32)
                result.type = STBIR_TYPE_FLOAT;
            else
                return std::unexpected("Resize supports unsigned 8/16-bit and float 16/32-bit samples; this format "
                                       "cannot be resized without losing precision");
            for (size_t i = 0; i < info.numChannles; ++i)
            {
                if (info.channles[i].width != first.width || info.channles[i].channelDataType != first.channelDataType)
                    return std::unexpected("Packed or mixed-width image formats are not supported by the scaler");
            }
            switch (image.GetTexelFormat())
            {
                case TexelFormat::I_R8_G8_B8:
                case TexelFormat::I_R16_G16_B16:
                case TexelFormat::F_R32_G32_B32:
                    result.layout = STBIR_RGB;
                    break;
                case TexelFormat::I_B8_G8_R8:
                case TexelFormat::I_B16_G16_R16:
                    result.layout = STBIR_BGR;
                    break;
                case TexelFormat::I_R8_G8_B8_A8:
                case TexelFormat::I_R16_G16_B16_A16:
                    result.layout = STBIR_RGBA;
                    break;
                case TexelFormat::I_B8_G8_R8_A8:
                case TexelFormat::I_B16_G16_R16_A16:
                    result.layout = STBIR_BGRA;
                    break;
                case TexelFormat::I_A8_R8_G8_B8:
                case TexelFormat::I_A16_R16_G16_B16:
                    result.layout = STBIR_ARGB;
                    break;
                case TexelFormat::I_A8_B8_G8_R8:
                case TexelFormat::I_A16_B16_G16_R16:
                    result.layout = STBIR_ABGR;
                    break;
                case TexelFormat::I_C8_M8_Y8_K8:
                    return std::unexpected("Convert CMYK to RGB before resizing");
                default:
                    if (info.numChannles != 1)
                        return std::unexpected("Unsupported pixel layout");
            }
            using ColorInfo   = ImageItem::ColorInfo;
            const auto& color = image.GetImageItem()->colorInfo;
            if (color.alphaMode == ColorInfo::AlphaMode::Premultiplied)
            {
                switch (result.layout)
                {
                    case STBIR_RGBA:
                        result.layout = STBIR_RGBA_PM;
                        break;
                    case STBIR_BGRA:
                        result.layout = STBIR_BGRA_PM;
                        break;
                    case STBIR_ARGB:
                        result.layout = STBIR_ARGB_PM;
                        break;
                    case STBIR_ABGR:
                        result.layout = STBIR_ABGR_PM;
                        break;
                    default:
                        break;
                }
            }
            if (result.type == STBIR_TYPE_UINT8 && color.transferFunction == ColorInfo::TransferFunction::SRGB &&
                !color.iccProfile && color.alphaMode != ColorInfo::AlphaMode::Premultiplied)
                result.type = STBIR_TYPE_UINT8_SRGB;
            return result;
        }
    }  // namespace
    std::expected<IMCodec::ImageSharedPtr, std::string> ResizeImage(const IMCodec::ImageSharedPtr& source,
                                                                    LLUtils::RectI32 crop,
                                                                    const ImageResizeRequest& request)
    {
        try
        {
            if (!source || source->GetWidth() > INT32_MAX || source->GetHeight() > INT32_MAX)
                return std::unexpected("Invalid source image dimensions");
            const LLUtils::RectI32 bounds{{0, 0}, static_cast<LLUtils::PointI32>(source->GetDimensions())};
            if (crop.IsEmpty() || !crop.IsInside(bounds))
                return std::unexpected("The source selection is outside the image");
            const auto layout = ResolveImageResize({crop.GetWidth(), crop.GetHeight()}, request);
            if (!layout)
                return std::unexpected(layout.error());
            const auto pixels = GetPixelLayout(*source);
            if (!pixels)
                return std::unexpected(pixels.error());
            const size_t bytes   = source->GetBytesPerTexel();
            const uint64_t pitch = uint64_t(layout->canvas.x) * bytes;
            if (pitch > UINT32_MAX || uint64_t(layout->canvas.y) > UINT32_MAX / pitch)
                return std::unexpected("Output dimensions exceed the supported allocation size");
            const uint64_t allocation  = pitch * layout->canvas.y;
            const uint64_t sourcePitch = source->GetRowPitchInBytes();
            if (sourcePitch < uint64_t(source->GetWidth()) * bytes ||
                (source->GetHeight() - 1) * sourcePitch + uint64_t(source->GetWidth()) * bytes >
                    source->GetImageItem()->data.size())
                return std::unexpected("Image dimensions or pixel strides exceed the supported allocation size");
            using Alpha         = IMCodec::ImageItem::ColorInfo::AlphaMode;
            const auto& info    = source->GetTexelInfo();
            const bool hasAlpha = std::ranges::any_of(
                std::span(info.channles).first(info.numChannles),
                [](const auto& channel) { return channel.semantic == IMCodec::ChannelSemantic::Opacity; });
            const bool transparentPadding = layout->content != layout->canvas && request.letterboxColor.A() < 255;
            if (transparentPadding && !hasAlpha)
            {
                // Resize only the selected content before adding alpha. Temporary storage is
                // proportional to the output, even when selecting from a very large source.
                if (info.channles[0].channelDataType != IMCodec::ChannelDataType::UnsignedInt)
                    return std::unexpected(
                        "Transparent letterboxing is unsupported for floating-point images without alpha");
                auto contentRequest   = request;
                contentRequest.size   = layout->content;
                contentRequest.aspect = ResizeAspectRatio::DoNotPreserve;
                auto content          = ResizeImage(source, crop, contentRequest);
                if (!content)
                    return std::unexpected(content.error());
                const size_t pixelBytes = 4 * pixels->channelBytes;
                const uint64_t newPitch = uint64_t(layout->canvas.x) * pixelBytes;
                if (newPitch > UINT32_MAX || uint64_t(layout->canvas.y) > UINT32_MAX / newPitch)
                    return std::unexpected("Adding alpha exceeds the supported allocation size");
                auto item                                = std::make_shared<IMCodec::ImageItem>();
                item->descriptor                         = source->GetImageItem()->descriptor;
                item->colorInfo                          = source->GetImageItem()->colorInfo;
                item->metaData                           = source->GetImageItem()->metaData;
                item->colorInfo.alphaMode                = Alpha::Straight;
                const bool bgr                           = pixels->layout == STBIR_BGR;
                item->descriptor.texelFormatDecompressed = pixels->channelBytes == 1
                                                               ? (bgr ? IMCodec::TexelFormat::I_B8_G8_R8_A8
                                                                      : IMCodec::TexelFormat::I_R8_G8_B8_A8)
                                                               : (bgr ? IMCodec::TexelFormat::I_B16_G16_R16_A16
                                                                      : IMCodec::TexelFormat::I_R16_G16_B16_A16);
                item->descriptor.width                   = layout->canvas.x;
                item->descriptor.height                  = layout->canvas.y;
                item->descriptor.rowPitchInBytes         = static_cast<uint32_t>(newPitch);
                item->data.Allocate(static_cast<size_t>(newPitch * layout->canvas.y));
                if (!item->data.data())
                    return std::unexpected("Not enough memory to add an alpha channel");
                const auto color = request.letterboxColor;
                const std::array<uint8_t, 4> channels{uint8_t(bgr ? color.B() : color.R()), uint8_t(color.G()),
                                                      uint8_t(bgr ? color.R() : color.B()), uint8_t(color.A())};
                std::array<std::byte, 8> background{};
                for (size_t c = 0; c < channels.size(); ++c)
                {
                    const uint16_t value = channels[c] * 257u;
                    std::memcpy(background.data() + c * pixels->channelBytes,
                                pixels->channelBytes == 1 ? static_cast<const void*>(&channels[c]) : &value,
                                pixels->channelBytes);
                }
                for (size_t i = 0; i < size_t(layout->canvas.x) * layout->canvas.y; ++i)
                    std::memcpy(item->data.data() + i * pixelBytes, background.data(), pixelBytes);
                for (int32_t y = 0; y < layout->content.y; ++y)
                    for (int32_t x = 0; x < layout->content.x; ++x)
                    {
                        const auto* input = (*content)->GetBuffer() + y * uint64_t((*content)->GetRowPitchInBytes()) +
                                            x * bytes;
                        auto* destination = item->data.data() + (y + layout->offset.y) * newPitch +
                                            (x + layout->offset.x) * pixelBytes;
                        for (size_t c = 0; c < 3; ++c)
                            std::memcpy(destination + c * pixels->channelBytes,
                                        input + (info.numChannles == 1 ? 0 : c * pixels->channelBytes),
                                        pixels->channelBytes);
                        std::memset(destination + 3 * pixels->channelBytes, 255, pixels->channelBytes);
                    }
                return std::make_shared<IMCodec::Image>(std::move(item), IMCodec::ImageItemType::Image);
            }
            auto output                        = std::make_shared<IMCodec::ImageItem>();
            output->descriptor                 = source->GetImageItem()->descriptor;
            output->descriptor.width           = layout->canvas.x;
            output->descriptor.height          = layout->canvas.y;
            output->descriptor.rowPitchInBytes = static_cast<uint32_t>(pitch);
            output->colorInfo                  = source->GetImageItem()->colorInfo;
            output->metaData                   = source->GetImageItem()->metaData;
            if (transparentPadding && output->colorInfo.alphaMode != Alpha::Premultiplied)
                output->colorInfo.alphaMode = Alpha::Straight;
            output->data.Allocate(static_cast<size_t>(allocation));
            auto* target = output->data.data();
            if (!target)
                return std::unexpected("Not enough memory for the resized image");
            if (layout->content != layout->canvas)
            {
                // Encode padding in the source channel order, sample precision, and alpha association.
                std::array<std::byte, 16> background{};
                const auto color   = request.letterboxColor;
                const float alpha  = color.A() / 255.f;
                const float weight = output->colorInfo.alphaMode == Alpha::Premultiplied ? alpha : 1.f;
                const float r = color.R() / 255.f * weight, g = color.G() / 255.f * weight,
                            b = color.B() / 255.f * weight;
                std::array<float, 4> channels{r, g, b, alpha};
                // Use the resolved layout, which also defines the scaler's channel interpretation.
                switch (pixels->layout)
                {
                    case STBIR_BGR:
                    case STBIR_BGRA:
                    case STBIR_BGRA_PM:
                        channels = {b, g, r, alpha};
                        break;
                    case STBIR_ARGB:
                    case STBIR_ARGB_PM:
                        channels = {alpha, r, g, b};
                        break;
                    case STBIR_ABGR:
                    case STBIR_ABGR_PM:
                        channels = {alpha, b, g, r};
                        break;
                    case STBIR_1CHANNEL:
                        channels[0] = 0.2126f * r + 0.7152f * g + 0.0722f * b;
                        break;
                    default:
                        break;
                }
                for (size_t channel = 0; channel < source->GetTexelInfo().numChannles; ++channel)
                {
                    const float value = channels[channel];
                    auto* sample      = background.data() + channel * pixels->channelBytes;
                    if (pixels->type == STBIR_TYPE_FLOAT)
                        std::memcpy(sample, &value, sizeof(value));
                    else if (pixels->type == STBIR_TYPE_HALF_FLOAT)
                    {
                        const half_float::half half(value);
                        std::memcpy(sample, &half, sizeof(half));
                    }
                    else if (pixels->channelBytes == 2)
                    {
                        const auto integer = static_cast<uint16_t>(std::lround(value * 65535));
                        std::memcpy(sample, &integer, sizeof(integer));
                    }
                    else
                        *sample = static_cast<std::byte>(std::lround(value * 255));
                }
                for (size_t i = 0; i < static_cast<size_t>(layout->canvas.x) * layout->canvas.y; ++i)
                    std::memcpy(target + i * bytes, background.data(), bytes);
            }
            const auto origin = crop.GetCorner(LLUtils::TopLeft);
            const auto* input = source->GetBuffer() + origin.y * sourcePitch + origin.x * bytes;
            auto* destination = target + layout->offset.y * pitch + layout->offset.x * bytes;
            if (layout->content.x == crop.GetWidth() && layout->content.y == crop.GetHeight())
            {
                for (int32_t y = 0; y < layout->content.y; ++y)
                    std::memcpy(destination + y * pitch, input + y * sourcePitch, layout->content.x * bytes);
            }
            else
            {
                STBIR_RESIZE resize;
                stbir_resize_init(&resize, input, crop.GetWidth(), crop.GetHeight(), sourcePitch, destination,
                                  layout->content.x, layout->content.y, pitch, pixels->layout, pixels->type);
                if (!stbir_set_filters(&resize, static_cast<stbir_filter>(layout->horizontal),
                                       static_cast<stbir_filter>(layout->vertical)) ||
                    !stbir_resize_extended(&resize))
                    return std::unexpected("The scaler could not allocate or process the image");
            }
            return std::make_shared<IMCodec::Image>(std::move(output), IMCodec::ImageItemType::Image);
        }
        catch (const std::exception& error)
        {
            return std::unexpected(std::string("Resize failed: ") + error.what());
        }
    }
}  // namespace OIV
