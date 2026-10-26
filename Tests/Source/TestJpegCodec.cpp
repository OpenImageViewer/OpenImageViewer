#include <IImagePlugin.h>
#include <ImageUtil/ImageUtil.h>
#include <ImageProcessing/FileImage.h>
#include <chrono>
#include <fstream>
#include "../../External/ImageCodec/Codecs/CodecJPG/Source/CodecJpeg.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

namespace
{
    using JpegBytes           = std::vector<unsigned char>;
    constexpr int ImageWidth  = 8;
    constexpr int ImageHeight = 8;

    JpegBytes MakeJpeg(int colorSpace, std::span<const unsigned char> pixel, bool progressive = false,
                       std::span<const unsigned char> iccProfile = {}, int width = ImageWidth, int height = ImageHeight)
    {
        const std::unique_ptr<void, decltype(&tj3Destroy)> handle(tj3Init(TJINIT_COMPRESS), tj3Destroy);
        REQUIRE(handle != nullptr);
        if (!iccProfile.empty())
            REQUIRE(tj3SetICCProfile(handle.get(), const_cast<unsigned char*>(iccProfile.data()), iccProfile.size()) ==
                    0);
        REQUIRE(tj3Set(handle.get(), TJPARAM_QUALITY, 100) == 0);
        REQUIRE(tj3Set(handle.get(), TJPARAM_SUBSAMP, colorSpace == TJCS_GRAY ? TJSAMP_GRAY : TJSAMP_444) == 0);
        REQUIRE(tj3Set(handle.get(), TJPARAM_COLORSPACE, colorSpace) == 0);
        REQUIRE(tj3Set(handle.get(), TJPARAM_PROGRESSIVE, progressive) == 0);
        const int pixelFormat = colorSpace == TJCS_GRAY                                ? TJPF_GRAY
                                : (colorSpace == TJCS_CMYK || colorSpace == TJCS_YCCK) ? TJPF_CMYK
                                                                                       : TJPF_RGB;
        REQUIRE(pixel.size() == tjPixelSize[pixelFormat]);
        JpegBytes pixels;
        for (int i = 0; i < width * height; ++i)
            pixels.insert(pixels.end(), pixel.begin(), pixel.end());

        unsigned char* buffer = nullptr;
        size_t size           = 0;
        const int result = tj3Compress8(handle.get(), pixels.data(), width, 0, height, pixelFormat, &buffer, &size);
        const std::unique_ptr<unsigned char, decltype(&tj3Free)> owner(buffer, tj3Free);
        INFO(tj3GetErrorStr(handle.get()));
        REQUIRE(result == 0);
        return {buffer, buffer + size};
    }

    size_t FindMarker(const JpegBytes& jpeg, unsigned char marker)
    {
        size_t offset = 2;
        while (offset + 4 <= jpeg.size())
        {
            REQUIRE(jpeg[offset] == 0xff);
            if (jpeg[offset + 1] == marker)
                return offset;
            REQUIRE(jpeg[offset + 1] != 0xda);
            offset += 2 + (static_cast<size_t>(jpeg[offset + 2]) << 8) + jpeg[offset + 3];
        }
        FAIL("JPEG fixture is missing marker " << static_cast<int>(marker));
        return 0;
    }

    IMCodec::ImageResult Decode(IMCodec::CodecJpeg& codec, const JpegBytes& jpeg, IMCodec::ImageSharedPtr& image)
    {
        return codec.Decode(reinterpret_cast<const std::byte*>(jpeg.data()), jpeg.size(), IMCodec::ImageLoadFlags::None,
                            {}, image);
    }

    void CheckPixels(const IMCodec::ImageSharedPtr& image, std::array<int, 3> rgb)
    {
        REQUIRE(image != nullptr);
        REQUIRE(image->GetWidth() == ImageWidth);
        REQUIRE(image->GetHeight() == ImageHeight);
        REQUIRE(image->GetTexelFormat() == IMCodec::TexelFormat::I_R8_G8_B8_A8);
        REQUIRE(image->GetRowPitchInBytes() == ImageWidth * 4);
        REQUIRE(image->GetImageItem()->data.size() == ImageWidth * ImageHeight * 4);
        const auto* pixels = reinterpret_cast<const unsigned char*>(image->GetBuffer());
        for (size_t offset = 0; offset < image->GetImageItem()->data.size(); offset += 4)
        {
            for (size_t channel = 0; channel < rgb.size(); ++channel)
                CHECK(std::abs(static_cast<int>(pixels[offset + channel]) - rgb[channel]) <= 2);
            CHECK(pixels[offset + 3] == 255);
        }
    }

    IMCodec::ImageSharedPtr MakeImage(IMCodec::TexelFormat format, std::span<const unsigned char> pixel)
    {
        auto item        = std::make_shared<IMCodec::ImageItem>();
        item->descriptor = {.width                   = ImageWidth,
                            .height                  = ImageHeight,
                            .rowPitchInBytes         = static_cast<uint32_t>(ImageWidth * pixel.size() + 5),
                            .texelFormatDecompressed = format};
        item->data.Allocate(item->descriptor.rowPitchInBytes * ImageHeight);
        REQUIRE(item->data.data() != nullptr);
        // Distinct padding makes an encoder that ignores row pitch produce incorrect pixels.
        std::memset(item->data.data(), 0x5a, item->data.size());
        for (size_t row = 0; row < ImageHeight; ++row)
            for (size_t column = 0; column < ImageWidth; ++column)
                std::memcpy(item->data.data() + row * item->descriptor.rowPitchInBytes + column * pixel.size(),
                            pixel.data(), pixel.size());
        return std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown);
    }
}  // namespace

TEST_CASE("JPEG decodes RGB, YCbCr and grayscale into opaque RGBA", "[ImageCompatibility][JPEG]")
{
    const int colorSpace   = GENERATE(TJCS_RGB, TJCS_YCbCr, TJCS_GRAY);
    const bool progressive = GENERATE(false, true);
    CAPTURE(colorSpace, progressive);
    const std::array<unsigned char, 3> color = {200, 100, 50};
    const auto jpeg = MakeJpeg(colorSpace, std::span(color).first(colorSpace == TJCS_GRAY ? 1 : 3), progressive);
    IMCodec::CodecJpeg codec;
    IMCodec::ImageSharedPtr image;
    REQUIRE(Decode(codec, jpeg, image) == IMCodec::ImageResult::Success);
    CheckPixels(image, colorSpace == TJCS_GRAY ? std::array{200, 200, 200} : std::array{200, 100, 50});
    CHECK(image->GetOriginalTexelFormat() ==
          (colorSpace == TJCS_GRAY ? IMCodec::TexelFormat::I_X8 : IMCodec::TexelFormat::I_R8_G8_B8));
    REQUIRE(image->GetImageItem()->metaData != nullptr);
    CHECK(image->GetImageItem()->metaData->sourceColorSpace ==
          (colorSpace == TJCS_GRAY  ? IMCodec::SourceColorSpace::Gray
           : colorSpace == TJCS_RGB ? IMCodec::SourceColorSpace::RGB
                                    : IMCodec::SourceColorSpace::YCbCr));
    CHECK(image->GetImageItem()->metaData->iccProfile.empty());
}

TEST_CASE("JPEG preserves Adobe CMYK and YCCK channels before display conversion", "[ImageCompatibility][JPEG]")
{
    const int colorSpace = GENERATE(TJCS_CMYK, TJCS_YCCK);
    const auto sample    = GENERATE(std::array<unsigned char, 4>{200, 100, 50, 128},
                                    std::array<unsigned char, 4>{255, 255, 255, 255},
                                    std::array<unsigned char, 4>{255, 255, 255, 0},
                                    std::array<unsigned char, 4>{255, 0, 0, 255});
    CAPTURE(colorSpace, sample);
    const auto jpeg    = MakeJpeg(colorSpace, sample);
    const size_t adobe = FindMarker(jpeg, 0xee);
    REQUIRE(std::memcmp(jpeg.data() + adobe + 4, "Adobe", 5) == 0);
    IMCodec::CodecJpeg codec;
    IMCodec::ImageSharedPtr image;
    REQUIRE(Decode(codec, jpeg, image) == IMCodec::ImageResult::Success);
    REQUIRE(image->GetTexelFormat() == IMCodec::TexelFormat::I_C8_M8_Y8_K8);
    CHECK(image->GetOriginalTexelFormat() == IMCodec::TexelFormat::I_C8_M8_Y8_K8);
    const auto& metadata = image->GetImageItem()->metaData;
    REQUIRE(metadata != nullptr);
    CHECK(metadata->sourceColorSpace ==
          (colorSpace == TJCS_CMYK ? IMCodec::SourceColorSpace::CMYK : IMCodec::SourceColorSpace::YCCK));
    CHECK(metadata->sourceCmykInverted);
    for (size_t offset = 0; offset < image->GetImageItem()->data.size(); ++offset)
        REQUIRE(std::abs(std::to_integer<int>(image->GetBuffer()[offset]) - (255 - sample[offset % 4])) <= 2);
    const auto display = IMUtil::ImageUtil::Convert(image, IMCodec::TexelFormat::I_R8_G8_B8_A8);
    REQUIRE(display != nullptr);
    CHECK(display->GetImageItem()->metaData == metadata);
    if (sample[3] == 128)
        CheckPixels(display, {100, 50, 25});
    else if (sample[3] == 0)
        CheckPixels(display, {0, 0, 0});
    else if (sample[1] == 0)
        CheckPixels(display, {255, 0, 0});
    else
        CheckPixels(display, {255, 255, 255});
}

TEST_CASE("JPEG distinguishes unmarked CMYK from Adobe-inverted CMYK", "[ImageCompatibility][JPEG]")
{
    const std::array<unsigned char, 4> ink = {55, 155, 205, 127};
    auto jpeg                              = MakeJpeg(TJCS_CMYK, ink);
    const size_t adobe                     = FindMarker(jpeg, 0xee);
    const size_t markerSize                = 2 + (static_cast<size_t>(jpeg[adobe + 2]) << 8) + jpeg[adobe + 3];
    jpeg.erase(jpeg.begin() + adobe, jpeg.begin() + adobe + markerSize);

    // An Adobe signature inside unrelated metadata must not change channel polarity.
    const auto unrelatedMetadata = GENERATE(false, true);
    if (unrelatedMetadata)
    {
        constexpr std::array<unsigned char, 20> comment = {0xff, 0xfe, 0,   18,  'X', 'X', 0xff, 0xee, 0, 14,
                                                           'A',  'd',  'o', 'b', 'e', 0,   100,  0,    0, 0};
        jpeg.insert(jpeg.begin() + 2, comment.begin(), comment.end());
    }
    IMCodec::CodecJpeg codec;
    IMCodec::ImageSharedPtr image;
    REQUIRE(Decode(codec, jpeg, image) == IMCodec::ImageResult::Success);
    REQUIRE(image->GetTexelFormat() == IMCodec::TexelFormat::I_C8_M8_Y8_K8);
    REQUIRE(image->GetImageItem()->metaData != nullptr);
    CHECK_FALSE(image->GetImageItem()->metaData->sourceCmykInverted);
    CHECK(image->GetImageItem()->metaData->sourceColorSpace == IMCodec::SourceColorSpace::CMYK);
    for (size_t offset = 0; offset < image->GetImageItem()->data.size(); ++offset)
        REQUIRE(std::abs(std::to_integer<int>(image->GetBuffer()[offset]) - ink[offset % 4]) <= 2);
    CheckPixels(IMUtil::ImageUtil::Convert(image, IMCodec::TexelFormat::I_R8_G8_B8_A8), {100, 50, 25});
}

TEST_CASE("JPEG rejects invalid data without replacing output and recovers on the next image", "[JPEG]")
{
    constexpr std::array<unsigned char, 3> color = {200, 100, 50};
    const auto valid                             = MakeJpeg(TJCS_YCbCr, color);
    auto invalid                                 = valid;
    SECTION("not a JPEG")
    {
        invalid = {'n', 'o', 't', 'j', 'p', 'e', 'g'};
    }
    SECTION("truncated header")
    {
        invalid.resize(12);
    }
    SECTION("truncated scan")
    {
        invalid.resize(FindMarker(invalid, 0xda) + 14);
    }
    SECTION("missing end marker")
    {
        invalid.resize(invalid.size() - 2);
    }
    SECTION("bad marker length")
    {
        const size_t marker = FindMarker(invalid, 0xdb);
        invalid[marker + 2] = 0;
        invalid[marker + 3] = 1;
    }
    SECTION("missing quantization table")
    {
        const size_t marker     = FindMarker(invalid, 0xdb);
        const size_t markerSize = 2 + (static_cast<size_t>(invalid[marker + 2]) << 8) + invalid[marker + 3];
        invalid.erase(invalid.begin() + marker, invalid.begin() + marker + markerSize);
    }
    SECTION("tables only")
    {
        invalid = {0xff, 0xd8, 0xff, 0xd9};
    }
    SECTION("fatal scan error")
    {
        // The SOS refers to a component that does not exist in the frame header.
        invalid[FindMarker(invalid, 0xda) + 5] = 99;
    }
    IMCodec::CodecJpeg codec;
    IMCodec::ImageSharedPtr image;
    REQUIRE(Decode(codec, valid, image) == IMCodec::ImageResult::Success);
    const auto previous = image;
    REQUIRE(Decode(codec, invalid, image) == IMCodec::ImageResult::FileIsCorrupted);
    CHECK(image == previous);
    REQUIRE(Decode(codec, valid, image) == IMCodec::ImageResult::Success);
    CheckPixels(image, {200, 100, 50});
}

TEST_CASE("JPEG validates null inputs and dimensions before allocating", "[JPEG]")
{
    IMCodec::CodecJpeg codec;
    IMCodec::ImageSharedPtr image;
    const std::byte byte{};
    CHECK(codec.Decode(nullptr, 10, IMCodec::ImageLoadFlags::None, {}, image) == IMCodec::ImageResult::BadParameters);
    CHECK(codec.Decode(&byte, 0, IMCodec::ImageLoadFlags::None, {}, image) == IMCodec::ImageResult::BadParameters);
    constexpr std::array<unsigned char, 3> color = {200, 100, 50};
    auto jpeg                                    = MakeJpeg(TJCS_YCbCr, color);
    const size_t frame                           = FindMarker(jpeg, 0xc0);
    // A valid JPEG dimension range can still overflow the image API's uint32_t slice size.
    jpeg[frame + 5] = jpeg[frame + 7] = 0xff;
    jpeg[frame + 6] = jpeg[frame + 8] = 0xdc;  // 65500, libjpeg's maximum supported dimension.
    CHECK(Decode(codec, jpeg, image) == IMCodec::ImageResult::FormatNotSupported);
    CHECK(image == nullptr);
}

TEST_CASE("JPEG reports unsupported precision explicitly", "[JPEG]")
{
    const std::unique_ptr<void, decltype(&tj3Destroy)> handle(tj3Init(TJINIT_COMPRESS), tj3Destroy);
    REQUIRE(handle != nullptr);
    REQUIRE(tj3Set(handle.get(), TJPARAM_QUALITY, 100) == 0);
    REQUIRE(tj3Set(handle.get(), TJPARAM_SUBSAMP, TJSAMP_GRAY) == 0);
    std::array<short, ImageWidth * ImageHeight> pixels{};
    pixels.fill(2048);
    unsigned char* buffer = nullptr;
    size_t size           = 0;
    const int result      = tj3Compress12(handle.get(), pixels.data(), ImageWidth, 0, ImageHeight, TJPF_GRAY, &buffer,
                                          &size);
    const std::unique_ptr<unsigned char, decltype(&tj3Free)> owner(buffer, tj3Free);
    REQUIRE(result == 0);
    IMCodec::CodecJpeg codec;
    IMCodec::ImageSharedPtr image;
    CHECK(Decode(codec, JpegBytes(buffer, buffer + size), image) == IMCodec::ImageResult::FormatNotSupported);
    CHECK(image == nullptr);
}

TEST_CASE("JPEG encodes supported byte layouts with padded rows", "[JPEG]")
{
    using enum IMCodec::TexelFormat;
    const auto format = GENERATE(I_R8_G8_B8, I_R8_G8_B8_A8, I_B8_G8_R8, I_B8_G8_R8_A8, I_A8_R8_G8_B8, I_A8_B8_G8_R8,
                                 I_X8);
    CAPTURE(format);
    IMCodec::CodecJpeg codec;
    const int pixelFormat              = codec.GetPixelFormat(format);
    std::array<unsigned char, 4> pixel = {0, 0, 0, 17};
    if (format == I_X8)
        pixel[0] = 200;
    else
    {
        pixel[tjRedOffset[pixelFormat]]   = 200;
        pixel[tjGreenOffset[pixelFormat]] = 100;
        pixel[tjBlueOffset[pixelFormat]]  = 50;
    }
    auto image = MakeImage(format, std::span(pixel).first(tjPixelSize[pixelFormat]));
    LLUtils::Buffer encoded;
    REQUIRE(codec.Encode(image, {}, encoded) == IMCodec::ImageResult::Success);
    IMCodec::ImageSharedPtr decoded;
    REQUIRE(codec.Decode(encoded.data(), encoded.size(), IMCodec::ImageLoadFlags::None, {}, decoded) ==
            IMCodec::ImageResult::Success);
    CheckPixels(decoded, format == I_X8 ? std::array{200, 200, 200} : std::array{200, 100, 50});
}

TEST_CASE("JPEG validates encoding parameters and keeps prior output on failure", "[JPEG]")
{
    constexpr std::array<unsigned char, 3> color = {200, 100, 50};
    auto image                                   = MakeImage(IMCodec::TexelFormat::I_R8_G8_B8, color);
    IMCodec::Parameters parameters;
    auto expected = IMCodec::ImageResult::BadParameters;
    SECTION("null image")
    {
        image.reset();
    }
    SECTION("zero width")
    {
        image->GetImageItem()->descriptor.width = 0;
    }
    SECTION("oversized dimensions")
    {
        image->GetImageItem()->descriptor.height = std::numeric_limits<uint32_t>::max();
    }
    SECTION("undersized pitch")
    {
        image->GetImageItem()->descriptor.rowPitchInBytes = 1;
    }
    SECTION("oversized pitch")
    {
        image->GetImageItem()->descriptor.rowPitchInBytes = std::numeric_limits<uint32_t>::max();
    }
    SECTION("short buffer")
    {
        image->GetImageItem()->data.Allocate(3);
    }
    SECTION("null buffer")
    {
        image->GetImageItem()->data.Free();
    }
    SECTION("unsupported format")
    {
        image->GetImageItem()->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R16_G16_B16;
        expected                                                  = IMCodec::ImageResult::FormatNotSupported;
    }
    SECTION("invalid quality type")
    {
        parameters.SetCustom(LLUTILS_TEXT("quality"), LLUTILS_TEXT("high"));
    }
    SECTION("negative quality")
    {
        parameters.SetCustom(LLUTILS_TEXT("quality"), -1);
    }
    SECTION("excessive quality")
    {
        parameters.SetCustom(LLUTILS_TEXT("quality"), 101.0);
    }
    SECTION("NaN quality")
    {
        parameters.SetCustom(LLUTILS_TEXT("quality"), std::bit_cast<double>(uint64_t{0x7ff8000000000000}));
    }
    SECTION("infinite quality")
    {
        parameters.SetCustom(LLUTILS_TEXT("quality"), std::bit_cast<double>(uint64_t{0x7ff0000000000000}));
    }
    const std::byte sentinel{0x42};
    LLUtils::Buffer encoded(&sentinel, 1);
    IMCodec::CodecJpeg codec;
    CHECK(codec.Encode(image, parameters, encoded) == expected);
    REQUIRE(encoded.size() == 1);
    CHECK(encoded.data()[0] == sentinel);
}

TEST_CASE("JPEG honors numeric quality including the advertised endpoints", "[JPEG]")
{
    constexpr std::array<unsigned char, 3> color = {200, 100, 50};
    const auto image                             = MakeImage(IMCodec::TexelFormat::I_R8_G8_B8, color);
    IMCodec::CodecJpeg codec;
    const bool useDouble = GENERATE(false, true);
    const int quality    = GENERATE(0, 1, 50, 80, 100);
    CAPTURE(useDouble, quality);
    IMCodec::Parameters parameters;
    if (useDouble)
        parameters.SetCustom(LLUTILS_TEXT("quality"), static_cast<double>(quality));
    else
        parameters.SetCustom(LLUTILS_TEXT("quality"), quality);
    LLUtils::Buffer encoded;
    REQUIRE(codec.Encode(image, parameters, encoded) == IMCodec::ImageResult::Success);
    const auto* bytes = reinterpret_cast<const unsigned char*>(encoded.data());
    JpegBytes jpeg(bytes, bytes + encoded.size());
    const size_t table = FindMarker(jpeg, 0xdb);
    // The first luminance quantizer has known values at these quality levels.
    const int quantizer = quality <= 1 ? 255 : quality == 50 ? 16 : quality == 80 ? 6 : 1;
    CHECK(jpeg[table + 5] == quantizer);
}

TEST_CASE("JPEG preserves complete ICC bytes for every supported source color space", "[JPEG][CMYK]")
{
    const int colorSpace = GENERATE(TJCS_RGB, TJCS_YCbCr, TJCS_GRAY, TJCS_CMYK, TJCS_YCCK);
    CAPTURE(colorSpace);
    // More than one APP2 marker verifies TurboJPEG reassembles the opaque profile in order.
    std::vector<unsigned char> profile(70000);
    for (size_t index = 0; index < profile.size(); ++index)
        profile[index] = static_cast<unsigned char>(index);
    constexpr std::array<unsigned char, 4> pixel = {200, 100, 50, 128};
    const size_t channels = colorSpace == TJCS_GRAY ? 1 : (colorSpace == TJCS_CMYK || colorSpace == TJCS_YCCK) ? 4 : 3;
    const auto jpeg       = MakeJpeg(colorSpace, std::span(pixel).first(channels), false, profile);
    IMCodec::CodecJpeg codec;
    IMCodec::ImageSharedPtr image;
    REQUIRE(Decode(codec, jpeg, image) == IMCodec::ImageResult::Success);
    const auto& metadata = image->GetImageItem()->metaData;
    REQUIRE(metadata != nullptr);
    REQUIRE(metadata->iccProfile.size() == profile.size());
    CHECK(std::memcmp(metadata->iccProfile.data(), profile.data(), profile.size()) == 0);
    const auto display = IMUtil::ImageUtil::Convert(image, IMCodec::TexelFormat::I_R8_G8_B8_A8);
    REQUIRE(display != nullptr);
    CHECK(display->GetImageItem()->metaData == metadata);

    // Saving the display image must not attach the source CMYK ICC profile to RGB JPEG pixels.
    LLUtils::Buffer encoded;
    REQUIRE(codec.Encode(display, {}, encoded) == IMCodec::ImageResult::Success);
    IMCodec::ImageSharedPtr exported;
    REQUIRE(codec.Decode(encoded.data(), encoded.size(), IMCodec::ImageLoadFlags::None, {}, exported) ==
            IMCodec::ImageResult::Success);
    REQUIRE(exported->GetImageItem()->metaData != nullptr);
    CHECK(exported->GetImageItem()->metaData->sourceColorSpace == IMCodec::SourceColorSpace::YCbCr);
    CHECK(exported->GetImageItem()->metaData->iccProfile.empty());
}

TEST_CASE("JPEG color metadata survives file loading with and without EXIF orientation", "[JPEG][CMYK][loading]")
{
    constexpr std::array<unsigned char, 4> pixel   = {200, 100, 50, 128};
    constexpr std::array<unsigned char, 5> profile = {1, 2, 3, 4, 5};
    const bool withExif                            = GENERATE(false, true);
    auto jpeg                                      = MakeJpeg(TJCS_CMYK, pixel, false, profile, 8, 4);
    if (withExif)
    {
        // Little-endian TIFF with one SHORT orientation entry: rotate 90 degrees clockwise.
        constexpr std::array<unsigned char, 36> exif = {0xff, 0xe1, 0, 34, 'E', 'x', 'i', 'f', 0,    0, 'I', 'I',
                                                        42,   0,    8, 0,  0,   0,   1,   0,   0x12, 1, 3,   0,
                                                        1,    0,    0, 0,  6,   0,   0,   0,   0,    0, 0,   0};
        jpeg.insert(jpeg.begin() + 2, exif.begin(), exif.end());
    }
    struct TemporaryFile
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
                                     ("oiv-cmyk-" +
                                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                                      ".jpg");
        ~TemporaryFile()
        {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } file;
    {
        std::ofstream output(file.path, std::ios::binary);
        output.write(reinterpret_cast<const char*>(jpeg.data()), jpeg.size());
        REQUIRE(output.good());
    }
    IMCodec::ImageLoader loader;
    IMCodec::ItemMetaDataSharedPtr metadata;
    const auto image = OIV::DecodeFileImage(loader, file.path.native(), metadata,
                                            IMCodec::PluginTraverseMode::NoTraverse);
    REQUIRE(image != nullptr);
    REQUIRE(metadata != nullptr);
    CHECK(image->GetImageItem()->metaData == metadata);
    CHECK(image->GetTexelFormat() == IMCodec::TexelFormat::I_C8_M8_Y8_K8);
    CHECK(image->GetWidth() == (withExif ? 4 : 8));
    CHECK(image->GetHeight() == (withExif ? 8 : 4));
    CHECK(metadata->sourceColorSpace == IMCodec::SourceColorSpace::CMYK);
    CHECK(metadata->sourceCmykInverted);
    CHECK(metadata->exifData.orientation == (withExif ? 6 : 0));
    REQUIRE(metadata->iccProfile.size() == profile.size());
    CHECK(std::memcmp(metadata->iccProfile.data(), profile.data(), profile.size()) == 0);
}

TEST_CASE("CMYK conversion, crop and transforms preserve padded source rows and metadata", "[CMYK]")
{
    constexpr std::array<unsigned char, 4> ink         = {55, 155, 205, 127};
    const auto source                                  = MakeImage(IMCodec::TexelFormat::I_C8_M8_Y8_K8, ink);
    source->GetImageItem()->metaData                   = std::make_shared<IMCodec::ItemMetaData>();
    source->GetImageItem()->metaData->sourceColorSpace = IMCodec::SourceColorSpace::CMYK;
    const auto metadata                                = source->GetImageItem()->metaData;
    const auto original                                = source->GetImageItem()->data.Clone();
    const auto display = IMUtil::ImageUtil::Convert(source, IMCodec::TexelFormat::I_R8_G8_B8_A8);
    CheckPixels(display, {100, 50, 25});
    CHECK(display->GetImageItem()->metaData == metadata);
    const auto bitmap = IMUtil::ImageUtil::ConvertImageWithNormalization(source, IMCodec::TexelFormat::I_B8_G8_R8_A8,
                                                                         false);
    REQUIRE(bitmap != nullptr);
    CHECK(bitmap->GetImageItem()->metaData == metadata);
    constexpr std::array bgra{std::byte{25}, std::byte{50}, std::byte{100}, std::byte{255}};
    for (size_t offset = 0; offset < bitmap->GetImageItem()->data.size(); offset += bgra.size())
        REQUIRE(std::equal(bgra.begin(), bgra.end(), bitmap->GetBuffer() + offset));

    const auto normalized = IMUtil::ImageUtil::NormalizePitch(source);
    REQUIRE(normalized != nullptr);
    CHECK(normalized->GetRowPitchInBytes() == ImageWidth * 4);
    CHECK(normalized->GetImageItem()->metaData == metadata);
    const auto cropped = IMUtil::ImageUtil::GetSubImage(source, {{1, 2}, {4, 6}});
    REQUIRE(cropped != nullptr);
    CHECK(cropped->GetWidth() == 3);
    CHECK(cropped->GetHeight() == 4);
    CHECK(cropped->GetRowPitchInBytes() == 12);
    CHECK(cropped->GetImageItem()->data.size() == 48);
    CHECK(cropped->GetImageItem()->metaData == metadata);
    const auto transformed = IMUtil::ImageUtil::Transform(
        {IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::Horizontal}, cropped);
    REQUIRE(transformed != nullptr);
    CHECK(transformed->GetWidth() == 4);
    CHECK(transformed->GetHeight() == 3);
    CHECK(transformed->GetImageItem()->metaData == metadata);
    for (size_t offset = 0; offset < transformed->GetImageItem()->data.size(); ++offset)
        REQUIRE(std::to_integer<unsigned char>(transformed->GetBuffer()[offset]) == ink[offset % 4]);
    CHECK(std::memcmp(source->GetBuffer(), original.data(), original.size()) == 0);
}

TEST_CASE("CMYK conversion covers uneven threaded image segments", "[CMYK]")
{
    auto item        = std::make_shared<IMCodec::ImageItem>();
    item->descriptor = {.width                   = 2049,
                        .height                  = 2049,
                        .rowPitchInBytes         = 2049 * 4,
                        .texelFormatDecompressed = IMCodec::TexelFormat::I_C8_M8_Y8_K8};
    item->data.Allocate(static_cast<size_t>(item->descriptor.rowPitchInBytes) * item->descriptor.height);
    REQUIRE(item->data.data() != nullptr);
    std::memset(item->data.data(), 0, item->data.size());
    const auto source  = std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown);
    const auto display = IMUtil::ImageUtil::Convert(source, IMCodec::TexelFormat::I_R8_G8_B8_A8);
    REQUIRE(display != nullptr);
    CHECK(std::ranges::all_of(std::span(display->GetBuffer(), display->GetImageItem()->data.size()),
                              [](std::byte value) { return value == std::byte{255}; }));
    CHECK(std::ranges::all_of(std::span(source->GetBuffer(), item->data.size()),
                              [](std::byte value) { return value == std::byte{0}; }));
}
