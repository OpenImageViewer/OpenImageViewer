#include <Image.h>
#include <ImageUtil/ImageUtil.h>
#include <ImageUtil/TexelConvertor.h>
#include <future>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <array>
#include <cstdint>
#include <limits>

TEST_CASE("Image byte sizes do not wrap at the 512 MiB bit-product boundary", "[ImageLayout][CMYK]")
{
    const auto height   = GENERATE(8191u, 8192u, 8193u);
    const auto expected = height == 8191 ? 536805376u : height == 8192 ? 536870912u : 536936448u;
    IMCodec::ImageDescriptor descriptor{16384, height, 65536, IMCodec::TexelFormat::I_C8_M8_Y8_K8,
                                        IMCodec::TexelFormat::I_C8_M8_Y8_K8};
    const IMCodec::ImageDesc image(&descriptor);
    CHECK(image.GetTotalSizeOfImageTexels() == expected);
    CHECK(image.GetSlicePitchInBytes() == expected);
    CHECK(image.GetSizeInMemory() == expected);
}

TEST_CASE("Shared image sizing preserves large byte and packed-bit layouts without allocating", "[ImageLayout]")
{
    struct Case
    {
        IMCodec::TexelFormat format;
        uint32_t rowBytes;
        uint32_t imageBytes;
    };
    const std::array cases{Case{IMCodec::TexelFormat::I_C8_M8_Y8_K8, 80000, 800000000},
                           Case{IMCodec::TexelFormat::I_R8_G8_B8_A8, 80000, 800000000},
                           Case{IMCodec::TexelFormat::I_R8_G8_B8, 60000, 600000000},
                           Case{IMCodec::TexelFormat::I_R16_G16_B16, 120000, 1200000000},
                           Case{IMCodec::TexelFormat::I_R16_G16_B16_A16, 160000, 1600000000},
                           Case{IMCodec::TexelFormat::I_X1, 2500, 25000000},
                           Case{IMCodec::TexelFormat::I_X4, 10000, 100000000}};
    for (const auto& test : cases)
    {
        CAPTURE(test.format);
        IMCodec::ImageDescriptor descriptor{20000, 10000, test.rowBytes, test.format, test.format};
        const IMCodec::ImageDesc image(&descriptor);
        CHECK(image.GetTotalPixels() == 200000000);
        CHECK(image.GetBytesPerRowOfPixels() == test.rowBytes);
        CHECK(image.GetRowPitchInTexels() == 20000);
        CHECK(image.GetSlicePitchInTexels() == 200000000);
        CHECK(image.GetTotalSizeOfImageTexels() == test.imageBytes);
        CHECK(image.GetSlicePitchInBytes() == test.imageBytes);
    }
    IMCodec::ImageDescriptor wide{200000000, 1, 800000000, IMCodec::TexelFormat::I_R8_G8_B8_A8,
                                  IMCodec::TexelFormat::I_R8_G8_B8_A8};
    const IMCodec::ImageDesc row(&wide);
    CHECK(row.GetBytesPerRowOfPixels() == 800000000);
    CHECK(row.GetRowPitchInTexels() == 200000000);
    IMCodec::ImageDescriptor large{32768, 24576, 131072, IMCodec::TexelFormat::I_C8_M8_Y8_K8,
                                   IMCodec::TexelFormat::I_C8_M8_Y8_K8};
    CHECK(IMCodec::ImageDesc(&large).GetTotalSizeOfImageTexels() == 3221225472u);
}

TEST_CASE("Image layout rejects values outside its 32-bit interface", "[ImageLayout]")
{
    constexpr auto maximum = std::numeric_limits<uint32_t>::max();
    IMCodec::ImageDescriptor descriptor{65536, 16384, 262144, IMCodec::TexelFormat::I_R8_G8_B8_A8,
                                        IMCodec::TexelFormat::I_R8_G8_B8_A8};
    IMCodec::ImageDesc image(&descriptor);
    CHECK(image.GetTotalPixels() == 1073741824);
    REQUIRE_THROWS_AS(image.GetTotalSizeOfImageTexels(), LLUtils::Exception);
    REQUIRE_THROWS_AS(image.GetSlicePitchInBytes(), LLUtils::Exception);
    REQUIRE_THROWS_AS(image.GetSizeInMemory(), LLUtils::Exception);
    descriptor.width = descriptor.height = 65536;
    REQUIRE_THROWS_AS(image.GetTotalPixels(), LLUtils::Exception);
    REQUIRE_THROWS_AS(image.GetTotalSizeOfImageTexels(), LLUtils::Exception);
    descriptor.width  = maximum;
    descriptor.height = 1;
    REQUIRE_THROWS_AS(image.GetBytesPerRowOfPixels(), LLUtils::Exception);
    descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_X1;
    descriptor.rowPitchInBytes         = maximum;
    REQUIRE_THROWS_AS(image.GetRowPitchInTexels(), LLUtils::Exception);
    descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_X8;
    descriptor.height                  = 2;
    REQUIRE_THROWS_AS(image.GetSlicePitchInTexels(), LLUtils::Exception);
}

TEST_CASE("Conversion validates the target layout before allocating or reading pixels", "[ImageLayout]")
{
    // The metadata alone requests 4 GiB of RGBA output. There is intentionally no source allocation:
    // the size rejection must precede both the allocation and any pixel traversal.
    auto item         = std::make_shared<IMCodec::ImageItem>();
    item->descriptor  = {.width                   = 65536,
                         .height                  = 16384,
                         .rowPitchInBytes         = 65536,
                         .texelFormatDecompressed = IMCodec::TexelFormat::I_X8};
    const auto source = std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown);
    REQUIRE_THROWS_AS(IMUtil::ImageUtil::Convert(source, IMCodec::TexelFormat::I_R8_G8_B8_A8), LLUtils::Exception);
    CHECK(source->GetImageItem()->data.data() == nullptr);
}

TEST_CASE("Fixed texel converters handle concurrent supported and unsupported lookups", "[ImageLayout][CMYK]")
{
    IMUtil::TexelConvertor converter;
    const auto convert = [&converter]
    {
        constexpr std::array source{std::byte{1}, std::byte{2}, std::byte{3}, std::byte{255}};
        constexpr std::array expected{std::byte{3}, std::byte{2}, std::byte{1}, std::byte{255}};
        bool valid = true;
        for (int iteration = 0; iteration < 100; ++iteration)
        {
            std::array<std::byte, 8> output{};
            valid = valid && !converter.Convert(IMCodec::TexelFormat::F_X64, IMCodec::TexelFormat::I_C8_M8_Y8_K8,
                                                output.data(), source.data(), 1);
            valid = valid && converter.Convert(IMCodec::TexelFormat::I_B8_G8_R8_A8, IMCodec::TexelFormat::I_R8_G8_B8_A8,
                                               output.data(), source.data(), 1);
            valid = valid && std::equal(expected.begin(), expected.end(), output.begin());
        }
        return valid;
    };
    auto first  = std::async(std::launch::async, convert);
    auto second = std::async(std::launch::async, convert);
    CHECK(first.get());
    CHECK(second.get());
}
