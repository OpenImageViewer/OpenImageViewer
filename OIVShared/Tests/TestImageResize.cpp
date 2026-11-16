#include <ImageProcessing/ImageResize.h>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cstring>
#include <limits>

namespace
{
    IMCodec::ImageSharedPtr Image(IMCodec::TexelFormat format, uint32_t width, uint32_t height, uint32_t padding = 0)
    {
        auto item        = std::make_shared<IMCodec::ImageItem>();
        const auto bytes = IMCodec::GetTexelFormatSize(format) / 8;
        item->descriptor = {.width                   = width,
                            .height                  = height,
                            .rowPitchInBytes         = width * bytes + padding,
                            .texelFormatDecompressed = format,
                            .texelFormatStorage      = format};
        item->data.Allocate(size_t(item->descriptor.rowPitchInBytes) * height);
        std::memset(item->data.data(), 0, item->data.size());
        return std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Image);
    }
    template <class T>
    T Sample(const IMCodec::ImageSharedPtr& image, size_t offset)
    {
        T value;
        std::memcpy(&value, image->GetBuffer() + offset, sizeof(T));
        return value;
    }
}  // namespace
TEST_CASE("Resize auto policy handles thresholds and independent axes", "[Shared][ImageResize]")
{
    using namespace OIV;
    CHECK(AutoResizeFilter(100, 1) == ResizeFilter::Mitchell);
    CHECK(AutoResizeFilter(100, 25) == ResizeFilter::Mitchell);
    CHECK(AutoResizeFilter(100, 26) == ResizeFilter::Mitchell);
    CHECK(AutoResizeFilter(100, 99) == ResizeFilter::Mitchell);
    CHECK(AutoResizeFilter(100, 100) == ResizeFilter::Point);
    CHECK(AutoResizeFilter(100, 101) == ResizeFilter::CatmullRom);
    CHECK(AutoResizeFilter(100, 200) == ResizeFilter::CatmullRom);
    CHECK(AutoResizeFilter(100, 201) == ResizeFilter::Mitchell);
    auto layout = ResolveImageResize({100, 100}, {.size = {50, 150}, .aspect = ResizeAspectRatio::DoNotPreserve});
    REQUIRE(layout);
    CHECK(layout->horizontal == ResizeFilter::Mitchell);
    CHECK(layout->vertical == ResizeFilter::CatmullRom);
}
TEST_CASE("Preset resize keeps dimensions by selecting black bars", "[Shared][ImageResize]")
{
    using namespace OIV;
    CHECK(PresetResizeAspect(ResizeAspectRatio::Preserve, {400, 300}, {1920, 1080}) == ResizeAspectRatio::BlackBars);
    CHECK(PresetResizeAspect(ResizeAspectRatio::Preserve, {400, 300}, {800, 600}) == ResizeAspectRatio::Preserve);
    CHECK(PresetResizeAspect(ResizeAspectRatio::DoNotPreserve, {400, 300}, {1920, 1080}) ==
          ResizeAspectRatio::DoNotPreserve);
    auto bars = ResolveImageResize({400, 300}, {.size = {1920, 1080}, .aspect = ResizeAspectRatio::BlackBars});
    REQUIRE(bars);
    CHECK(bars->canvas == LLUtils::PointI32{1920, 1080});
    CHECK(bars->content == LLUtils::PointI32{1440, 1080});
    CHECK(bars->offset == LLUtils::PointI32{240, 0});
    auto preserve = ResolveImageResize({400, 300}, {.size = {800, 800}});
    REQUIRE(preserve);
    CHECK(preserve->canvas == LLUtils::PointI32{800, 600});
}
TEST_CASE("Resize identity and crop retain exact samples across padded rows", "[Shared][ImageResize]")
{
    auto source = Image(IMCodec::TexelFormat::I_X16, 4, 3, 6);
    for (size_t y = 0; y < 3; ++y)
        for (size_t x = 0; x < 4; ++x)
        {
            const uint16_t value = static_cast<uint16_t>(257 * x + 1043 * y);
            std::memcpy(source->GetImageItem()->data.data() + y * 14 + x * 2, &value, 2);
        }
    auto full = OIV::ResizeImage(source, {{0, 0}, {4, 3}}, {.size = {4, 3}});
    REQUIRE(full);
    auto crop = OIV::ResizeImage(source, {{1, 1}, {3, 3}}, {.size = {2, 2}});
    REQUIRE(crop);
    for (size_t y = 0; y < 2; ++y)
        for (size_t x = 0; x < 2; ++x)
            CHECK(Sample<uint16_t>(*crop, y * 4 + x * 2) == Sample<uint16_t>(source, (y + 1) * 14 + (x + 1) * 2));
    CHECK(Sample<uint16_t>(*full, 16) == Sample<uint16_t>(source, 28));
}
TEST_CASE("Every built-in scaler retains 16-bit precision and HDR range", "[Shared][ImageResize]")
{
    auto integer         = Image(IMCodec::TexelFormat::I_X16, 3, 2);
    auto hdr             = Image(IMCodec::TexelFormat::F_R32_G32_B32, 3, 2);
    const uint16_t value = 12345;
    const float bright   = 8.25f;
    for (size_t i = 0; i < 6; ++i)
        std::memcpy(integer->GetImageItem()->data.data() + i * 2, &value, 2);
    for (size_t i = 0; i < 18; ++i)
        std::memcpy(hdr->GetImageItem()->data.data() + i * 4, &bright, 4);
    for (auto filter : OIV::ResizeFilters)
    {
        const OIV::ImageResizeRequest request{.size       = {7, 5},
                                              .aspect     = OIV::ResizeAspectRatio::DoNotPreserve,
                                              .autoFilter = false,
                                              .filter     = filter};
        auto a = OIV::ResizeImage(integer, {{0, 0}, {3, 2}}, request);
        auto b = OIV::ResizeImage(hdr, {{0, 0}, {3, 2}}, request);
        REQUIRE(a);
        REQUIRE(b);
        CHECK((*a)->GetTexelFormat() == IMCodec::TexelFormat::I_X16);
        CHECK(Sample<uint16_t>(*a, 0) == value);
        CHECK(Sample<float>(*b, 0) == Catch::Approx(bright).margin(.0001));
    }
}
TEST_CASE("Black bars are opaque and preserve the centered image", "[Shared][ImageResize]")
{
    auto source = Image(IMCodec::TexelFormat::I_R8_G8_B8_A8, 2, 1);
    const uint8_t pixels[]{255, 0, 0, 128, 255, 0, 0, 128};
    std::memcpy(source->GetImageItem()->data.data(), pixels, sizeof(pixels));
    auto result = OIV::ResizeImage(source, {{0, 0}, {2, 1}},
                                   {.size = {2, 3}, .aspect = OIV::ResizeAspectRatio::BlackBars});
    REQUIRE(result);
    CHECK(Sample<uint32_t>(*result, 0) == 0xff000000u);
    CHECK(Sample<uint8_t>(*result, 8) == 255);
    CHECK(Sample<uint8_t>(*result, 11) == 128);
    CHECK(Sample<uint32_t>(*result, 16) == 0xff000000u);
}
TEST_CASE("Letterbox colors retain channel order and precision", "[Shared][ImageResize]")
{
    for (auto format : {IMCodec::TexelFormat::I_R8_G8_B8_A8, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                        IMCodec::TexelFormat::I_R16_G16_B16_A16, IMCodec::TexelFormat::F_R32_G32_B32})
    {
        CAPTURE(format);
        auto source = Image(format, 2, 1);
        auto result = OIV::ResizeImage(source, {{0, 0}, {2, 1}},
                                       {.size           = {2, 3},
                                        .aspect         = OIV::ResizeAspectRatio::BlackBars,
                                        .letterboxColor = LLUtils::Color(255, 128, 0)});
        REQUIRE(result);
        CHECK((*result)->GetTexelFormat() == format);
        if (format == IMCodec::TexelFormat::F_R32_G32_B32)
        {
            CHECK(Sample<float>(*result, 0) == 1.f);
            CHECK(Sample<float>(*result, 4) == Catch::Approx(128.f / 255));
            CHECK(Sample<float>(*result, 8) == 0.f);
        }
        else if (format == IMCodec::TexelFormat::I_R16_G16_B16_A16)
        {
            CHECK(Sample<uint16_t>(*result, 0) == 65535);
            CHECK(Sample<uint16_t>(*result, 2) == 32896);
            CHECK(Sample<uint16_t>(*result, 4) == 0);
            CHECK(Sample<uint16_t>(*result, 6) == 65535);
        }
        else
        {
            const bool bgr = format == IMCodec::TexelFormat::I_B8_G8_R8_A8;
            CHECK(Sample<uint8_t>(*result, bgr ? 2 : 0) == 255);
            CHECK(Sample<uint8_t>(*result, 1) == 128);
            CHECK(Sample<uint8_t>(*result, bgr ? 0 : 2) == 0);
            CHECK(Sample<uint8_t>(*result, 3) == 255);
        }
    }
}
TEST_CASE("Half-float letterboxing encodes normalized grayscale", "[Shared][ImageResize]")
{
    auto source = Image(IMCodec::TexelFormat::F_X16, 2, 1);
    auto result = OIV::ResizeImage(
        source, {{0, 0}, {2, 1}},
        {.size = {2, 3}, .aspect = OIV::ResizeAspectRatio::BlackBars, .letterboxColor = LLUtils::Color(255, 255, 255)});
    REQUIRE(result);
    CHECK(Sample<uint16_t>(*result, 0) == 0x3c00);
    CHECK(Sample<uint16_t>(*result, 8) == 0x3c00);
}
TEST_CASE("Letterbox alpha is retained in straight and premultiplied output", "[Shared][ImageResize]")
{
    using Alpha = IMCodec::ImageItem::ColorInfo::AlphaMode;
    for (auto mode : {Alpha::Straight, Alpha::Premultiplied, Alpha::Opaque})
        for (uint8_t alpha : {uint8_t{0}, uint8_t{128}, uint8_t{255}})
        {
            CAPTURE(mode, alpha);
            auto source                                 = Image(IMCodec::TexelFormat::I_R8_G8_B8_A8, 2, 1);
            source->GetImageItem()->colorInfo.alphaMode = mode;
            const uint8_t content[]{20, 40, 60, 255, 20, 40, 60, 255};
            std::memcpy(source->GetImageItem()->data.data(), content, sizeof(content));
            auto result = OIV::ResizeImage(source, {{0, 0}, {2, 1}},
                                           {.size           = {2, 3},
                                            .aspect         = OIV::ResizeAspectRatio::BlackBars,
                                            .letterboxColor = LLUtils::Color(255, 0, 0, static_cast<int>(alpha))});
            REQUIRE(result);
            CHECK(Sample<uint8_t>(*result, 0) == (mode == Alpha::Premultiplied ? alpha : 255));
            CHECK(Sample<uint8_t>(*result, 3) == alpha);
            CHECK(Sample<uint8_t>(*result, 19) == alpha);
            CHECK(Sample<uint32_t>(*result, 8) == Sample<uint32_t>(source, 0));
            if (alpha < 255)
                CHECK((*result)->GetImageItem()->colorInfo.alphaMode ==
                      (mode == Alpha::Premultiplied ? Alpha::Premultiplied : Alpha::Straight));
        }
}
TEST_CASE("Transparent padding adds alpha without losing integer source precision", "[Shared][ImageResize]")
{
    for (auto format : {IMCodec::TexelFormat::I_R8_G8_B8, IMCodec::TexelFormat::I_B8_G8_R8,
                        IMCodec::TexelFormat::I_R16_G16_B16, IMCodec::TexelFormat::I_X16})
    {
        CAPTURE(format);
        auto source = Image(format, 2, 1);
        std::memset(source->GetImageItem()->data.data(), 85, source->GetImageItem()->data.size());
        auto result = OIV::ResizeImage(source, {{0, 0}, {2, 1}},
                                       {.size           = {2, 3},
                                        .aspect         = OIV::ResizeAspectRatio::BlackBars,
                                        .letterboxColor = LLUtils::Color(0, 0, 255, 128)});
        REQUIRE(result);
        const size_t channelBytes = source->GetTexelInfo().channles[0].width / 8;
        CHECK((*result)->GetBytesPerTexel() == 4 * channelBytes);
        if (channelBytes == 1)
        {
            CHECK(Sample<uint8_t>(*result, 3) == 128);
            CHECK(Sample<uint8_t>(*result, 8) == 85);
            CHECK(Sample<uint8_t>(*result, 11) == 255);
        }
        else
        {
            CHECK(Sample<uint16_t>(*result, 6) == 32896);
            CHECK(Sample<uint16_t>(*result, 16) == 21845);
            CHECK(Sample<uint16_t>(*result, 22) == 65535);
        }
    }
    auto floating = Image(IMCodec::TexelFormat::F_R32_G32_B32, 2, 1);
    CHECK_FALSE(OIV::ResizeImage(
        floating, {{0, 0}, {2, 1}},
        {.size = {2, 3}, .aspect = OIV::ResizeAspectRatio::BlackBars, .letterboxColor = LLUtils::Color(0, 0, 0, 0)}));
}
TEST_CASE("Straight alpha filtering avoids hidden-color fringes", "[Shared][ImageResize]")
{
    auto source                                 = Image(IMCodec::TexelFormat::I_R8_G8_B8_A8, 2, 1);
    source->GetImageItem()->colorInfo.alphaMode = IMCodec::ImageItem::ColorInfo::AlphaMode::Straight;
    const uint8_t pixels[]{255, 0, 0, 255, 0, 0, 255, 0};
    std::memcpy(source->GetImageItem()->data.data(), pixels, sizeof(pixels));
    auto result = OIV::ResizeImage(source, {{0, 0}, {2, 1}},
                                   {.size       = {1, 1},
                                    .aspect     = OIV::ResizeAspectRatio::DoNotPreserve,
                                    .autoFilter = false,
                                    .filter     = OIV::ResizeFilter::Box});
    REQUIRE(result);
    CHECK(Sample<uint8_t>(*result, 0) > 250);
    CHECK(Sample<uint8_t>(*result, 2) < 5);
    CHECK(Sample<uint8_t>(*result, 3) == Catch::Approx(128).margin(1));
}
TEST_CASE("Resize rejects invalid extents, allocation sizes, and unsupported precision", "[Shared][ImageResize]")
{
    auto source = Image(IMCodec::TexelFormat::I_R8_G8_B8_A8, 2, 2);
    CHECK_FALSE(OIV::ResizeImage(source, {{0, 0}, {2, 2}}, {.size = {0, 4}}));
    CHECK_FALSE(OIV::ResizeImage(source, {{0, 0}, {3, 2}}, {.size = {4, 4}}));
    CHECK_FALSE(OIV::ResizeImage(source, {{0, 0}, {2, 2}}, {.size = {INT32_MAX, INT32_MAX}}));
    CHECK_FALSE(OIV::ResizeImage(source, {{0, 0}, {2, 2}}, {.size = {4, 4}, .dpi = 0}));
    auto unsupported = Image(IMCodec::TexelFormat::F_X64, 2, 2);
    CHECK_FALSE(OIV::ResizeImage(unsupported, {{0, 0}, {2, 2}}, {.size = {4, 4}}));
}

TEST_CASE("Uniform pixel rounding retains the requested axis", "[Shared][ImageResize]")
{
    using namespace OIV;
    const auto byHeight = ResolveImageResize({130, 260}, {.size = {261, 521}, .preserveHeight = true});
    REQUIRE(byHeight);
    CHECK(byHeight->canvas == LLUtils::PointI32{261, 521});
    const auto byWidth = ResolveImageResize({130, 260}, {.size = {261, 521}});
    REQUIRE(byWidth);
    CHECK(byWidth->canvas == LLUtils::PointI32{261, 522});
    const auto independent = ResolveImageResize(
        {130, 260}, {.size = {261, 521}, .aspect = ResizeAspectRatio::DoNotPreserve, .preserveHeight = true});
    REQUIRE(independent);
    CHECK(independent->canvas == LLUtils::PointI32{261, 521});
    CHECK_FALSE(ResolveImageResize({2147483647, 1}, {.size = {1, 2}, .preserveHeight = true}));
}

TEST_CASE("Transparent padding resizes the selected content before alpha promotion", "[Shared][ImageResize]")
{
    auto source = Image(IMCodec::TexelFormat::I_R16_G16_B16, 40, 30, 8);
    const uint16_t pixel[]{1234, 2345, 3456};
    for (uint32_t y = 7; y < 11; ++y)
        for (uint32_t x = 13; x < 21; ++x)
            std::memcpy(source->GetImageItem()->data.data() + y * source->GetRowPitchInBytes() + x * 6, pixel,
                        sizeof(pixel));
    auto result = OIV::ResizeImage(source, {{13, 7}, {21, 11}},
                                   {.size           = {4, 6},
                                    .aspect         = OIV::ResizeAspectRatio::BlackBars,
                                    .autoFilter     = false,
                                    .filter         = OIV::ResizeFilter::Box,
                                    .letterboxColor = LLUtils::Color(10, 20, 30, 40)});
    REQUIRE(result);
    CHECK((*result)->GetWidth() == 4);
    CHECK((*result)->GetHeight() == 6);
    CHECK(Sample<uint16_t>(*result, 0) == 2570);
    CHECK(Sample<uint16_t>(*result, 6) == 10280);
    const size_t content = 2 * (*result)->GetRowPitchInBytes();
    CHECK(Sample<uint16_t>(*result, content) == 1234);
    CHECK(Sample<uint16_t>(*result, content + 2) == 2345);
    CHECK(Sample<uint16_t>(*result, content + 4) == 3456);
    CHECK(Sample<uint16_t>(*result, content + 6) == 65535);
}
