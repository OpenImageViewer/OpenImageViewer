#include <ImageLoader.h>
#include "../../External/ImageCodec/Codecs/CodecPNG/Include/CodecPNGFactory.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <ImageUtil/ImageUtil.h>
#include <memory>
#include <limits>
#include <vector>

namespace
{
    using Bytes = std::vector<uint8_t>;

    void Big32(Bytes& bytes, uint32_t value)
    {
        for (int shift = 24; shift >= 0; shift -= 8)
            bytes.push_back(static_cast<uint8_t>(value >> shift));
    }

    void Chunk(Bytes& png, const char* type, const Bytes& payload)
    {
        Big32(png, static_cast<uint32_t>(payload.size()));
        const auto start = png.size();
        png.insert(png.end(), type, type + 4);
        png.insert(png.end(), payload.begin(), payload.end());
        uint32_t crc = 0xffffffff;
        for (size_t i = start; i < png.size(); ++i)
        {
            crc ^= png[i];
            for (unsigned bit = 0; bit < 8; ++bit)
                crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1)));
        }
        Big32(png, ~crc);
    }

    // Construct PNG chunks and stored DEFLATE blocks independently of libpng,
    // so decoder assertions do not depend on matching encoder behavior.
    Bytes Deflate(const Bytes& raw)
    {
        REQUIRE(raw.size() < 65536);
        const auto size = static_cast<uint16_t>(raw.size());
        Bytes result{0x78,
                     0x01,
                     0x01,
                     static_cast<uint8_t>(size),
                     static_cast<uint8_t>(size >> 8),
                     static_cast<uint8_t>(~size),
                     static_cast<uint8_t>(~size >> 8)};
        result.insert(result.end(), raw.begin(), raw.end());
        uint32_t a = 1, b = 0;
        for (const auto value : raw)
        {
            a = (a + value) % 65521;
            b = (b + a) % 65521;
        }
        Big32(result, (b << 16) | a);
        return result;
    }

    Bytes Header(uint32_t width, uint32_t height, uint8_t depth, uint8_t color, bool interlaced = false)
    {
        Bytes png{137, 80, 78, 71, 13, 10, 26, 10};
        Bytes header;
        Big32(header, width);
        Big32(header, height);
        header.insert(header.end(), {depth, color, 0, 0, static_cast<uint8_t>(interlaced)});
        Chunk(png, "IHDR", header);
        return png;
    }

    void InsertChunk(Bytes& png, const char* type, const Bytes& payload)
    {
        Bytes chunk;
        Chunk(chunk, type, payload);
        // Color chunks precede image data and the optional animation header.
        png.insert(png.begin() + 33, chunk.begin(), chunk.end());
    }

    Bytes ICCProfile(const char* name = "test3.icc")
    {
        const auto path = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                          "External/ImageCodec/External/libjpeg-turbo/testimages" / name;
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        REQUIRE(file.is_open());
        const auto size = file.tellg();
        REQUIRE(size > 0);
        Bytes bytes(static_cast<size_t>(size));
        file.seekg(0);
        REQUIRE(file.read(reinterpret_cast<char*>(bytes.data()), size).good());
        return bytes;
    }

    void InsertProfile(Bytes& png, const Bytes& profile)
    {
        Bytes payload{'t', 'e', 's', 't', 0, 0};
        const auto compressed = Deflate(profile);
        payload.insert(payload.end(), compressed.begin(), compressed.end());
        InsertChunk(png, "iCCP", payload);
    }

    Bytes Png(uint8_t depth, uint8_t color, const Bytes& row, const Bytes& transparency = {}, const Bytes& palette = {},
              bool gamma = false)
    {
        auto png = Header(2, 1, depth, color);
        if (gamma)
        {
            Bytes value;
            Big32(value, 100000);
            Chunk(png, "gAMA", value);
            Chunk(png, "sRGB", {0});
        }
        if (!palette.empty())
            Chunk(png, "PLTE", palette);
        if (!transparency.empty())
            Chunk(png, "tRNS", transparency);
        Bytes raw{0};
        raw.insert(raw.end(), row.begin(), row.end());
        Chunk(png, "IDAT", Deflate(raw));
        Chunk(png, "IEND", {});
        return png;
    }

    std::unique_ptr<IMCodec::IImagePlugin> Codec()
    {
        return std::unique_ptr<IMCodec::IImagePlugin>(IMCodec::CodecPNGFactory::Create());
    }

    IMCodec::ImageResult Decode(const Bytes& png, IMCodec::ImageSharedPtr& image,
                                IMCodec::ImageLoadFlags flags = IMCodec::ImageLoadFlags::None)
    {
        return Codec()->Decode(reinterpret_cast<const std::byte*>(png.data()), png.size(), flags, {}, image);
    }

    void CheckBytes(const IMCodec::ImageSharedPtr& image, const Bytes& expected)
    {
        REQUIRE(image->GetImageItem()->data.size() == expected.size());
        REQUIRE(std::memcmp(image->GetBuffer(), expected.data(), expected.size()) == 0);
    }

    void CheckWords(const IMCodec::ImageSharedPtr& image, const std::vector<uint16_t>& expected)
    {
        REQUIRE(image->GetImageItem()->data.size() == expected.size() * sizeof(uint16_t));
        REQUIRE(std::memcmp(image->GetBuffer(), expected.data(), expected.size() * sizeof(uint16_t)) == 0);
    }

    struct Frame
    {
        uint32_t width = 2, height = 1, x = 0, y = 0;
        uint16_t numerator = 1, denominator = 10;
        uint8_t dispose = 0, blend = 0;
        Bytes pixels;
    };

    Bytes Scanlines(const Bytes& pixels, uint32_t width, uint32_t height, unsigned pixelBytes, bool interlaced)
    {
        constexpr std::array<unsigned, 7> xStart{0, 4, 0, 2, 0, 1, 0}, yStart{0, 0, 4, 0, 2, 0, 1};
        constexpr std::array<unsigned, 7> xStep{8, 8, 4, 4, 2, 2, 1}, yStep{8, 8, 8, 4, 4, 2, 2};
        Bytes raw;
        for (size_t pass = 0; pass < (interlaced ? 7u : 1u); ++pass)
        {
            const unsigned startX = interlaced ? xStart[pass] : 0;
            const unsigned startY = interlaced ? yStart[pass] : 0;
            const unsigned stepX  = interlaced ? xStep[pass] : 1;
            const unsigned stepY  = interlaced ? yStep[pass] : 1;
            if (startX < width)
                for (unsigned y = startY; y < height; y += stepY)
                {
                    raw.push_back(0);
                    for (unsigned x = startX; x < width; x += stepX)
                    {
                        const auto start = (y * width + x) * pixelBytes;
                        REQUIRE(start + pixelBytes <= pixels.size());
                        raw.insert(raw.end(), pixels.begin() + start, pixels.begin() + start + pixelBytes);
                    }
                }
        }
        return raw;
    }

    Bytes Animation(const std::vector<Frame>& frames, bool hidden = false, uint8_t depth = 8, uint32_t width = 2,
                    uint32_t height = 1, bool interlaced = false)
    {
        auto png = Header(width, height, depth, 6, interlaced);
        Bytes control;
        Big32(control, static_cast<uint32_t>(frames.size()));
        Big32(control, 0);
        Chunk(png, "acTL", control);
        if (hidden)
        {
            Bytes pixels(width * height * 4 * (depth / 8), 0);
            Chunk(png, "IDAT", Deflate(Scanlines(pixels, width, height, 4 * (depth / 8), interlaced)));
        }
        uint32_t sequence = 0;
        for (size_t index = 0; index < frames.size(); ++index)
        {
            const auto& frame = frames[index];
            Bytes ctl;
            Big32(ctl, sequence++);
            for (const auto value : {frame.width, frame.height, frame.x, frame.y})
                Big32(ctl, value);
            ctl.insert(ctl.end(), {static_cast<uint8_t>(frame.numerator >> 8), static_cast<uint8_t>(frame.numerator),
                                   static_cast<uint8_t>(frame.denominator >> 8),
                                   static_cast<uint8_t>(frame.denominator), frame.dispose, frame.blend});
            Chunk(png, "fcTL", ctl);
            auto compressed = Deflate(Scanlines(frame.pixels, frame.width, frame.height, 4 * (depth / 8), interlaced));
            if (index == 0 && !hidden)
                Chunk(png, "IDAT", compressed);
            else
            {
                Bytes data;
                Big32(data, sequence++);
                data.insert(data.end(), compressed.begin(), compressed.end());
                Chunk(png, "fdAT", data);
            }
        }
        Chunk(png, "IEND", {});
        return png;
    }
}  // namespace

TEST_CASE("PNG preserves stored samples without gamma or alpha conversion", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    ImageSharedPtr image;
    const Bytes rgba{17, 33, 99, 0, 101, 127, 201, 129};
    for (bool gamma : {false, true})
    {
        auto png = Png(8, 6, rgba, {}, {}, gamma);
        REQUIRE(Decode(png, image) == ImageResult::Success);
        REQUIRE(image->GetTexelFormat() == TexelFormat::I_R8_G8_B8_A8);
        std::fill(png.begin(), png.end(), 0);
        CheckBytes(image, rgba);
        const Bytes words{0x12, 0x34, 0xab, 0xcd, 0x01, 0x23, 0, 0, 0x56, 0x78, 0x23, 0x45, 0xfe, 0xdc, 0x80, 0x01};
        REQUIRE(Decode(Png(16, 6, words, {}, {}, gamma), image) == ImageResult::Success);
        REQUIRE(image->GetTexelFormat() == TexelFormat::I_R16_G16_B16_A16);
        CheckWords(image, {0x1234, 0xabcd, 0x0123, 0, 0x5678, 0x2345, 0xfedc, 0x8001});
    }
    REQUIRE(Decode(Png(16, 2, {0x12, 0x34, 0xab, 0xcd, 0, 1, 0x56, 0x78, 0x23, 0x45, 0xfe, 0xdc}), image) ==
            ImageResult::Success);
    REQUIRE(image->GetTexelFormat() == TexelFormat::I_R16_G16_B16);
    CheckWords(image, {0x1234, 0xabcd, 1, 0x5678, 0x2345, 0xfedc});
}

TEST_CASE("PNG expands palette and grayscale storage without losing transparency", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    ImageSharedPtr image;
    REQUIRE(Decode(Png(1, 3, {0x40}, {0, 128}, {17, 23, 31, 101, 127, 201}), image) == ImageResult::Success);
    CheckBytes(image, {17, 23, 31, 0, 101, 127, 201, 128});
    REQUIRE(Decode(Png(1, 3, {0x40}, {}, {17, 23, 31, 101, 127, 201}), image) == ImageResult::Success);
    REQUIRE(image->GetTexelFormat() == TexelFormat::I_R8_G8_B8);
    CheckBytes(image, {17, 23, 31, 101, 127, 201});
    for (const auto depth : {1, 2, 4})
    {
        REQUIRE(
            Decode(Png(static_cast<uint8_t>(depth), 0, {static_cast<uint8_t>(((1 << depth) - 1) << (8 - 2 * depth))}),
                   image) == ImageResult::Success);
        REQUIRE(image->GetTexelFormat() == TexelFormat::I_X8);
        CheckBytes(image, {0, 255});
    }
    REQUIRE(Decode(Png(8, 4, {17, 0, 99, 128}), image) == ImageResult::Success);
    CheckBytes(image, {17, 17, 17, 0, 99, 99, 99, 128});
    REQUIRE(Decode(Png(8, 0, {17, 99}, {0, 17}), image) == ImageResult::Success);
    CheckBytes(image, {17, 17, 17, 0, 99, 99, 99, 255});
    REQUIRE(Decode(Png(16, 0, {0x12, 0x34, 0xab, 0xcd}), image) == ImageResult::Success);
    REQUIRE(image->GetTexelFormat() == TexelFormat::I_X16);
    CheckWords(image, {0x1234, 0xabcd});
    REQUIRE(Decode(Png(16, 4, {0x12, 0x34, 0, 0, 0xab, 0xcd, 0x80, 0}), image) == ImageResult::Success);
    CheckWords(image, {0x1234, 0x1234, 0x1234, 0, 0xabcd, 0xabcd, 0xabcd, 0x8000});
    REQUIRE(Decode(Png(8, 2, {17, 23, 31, 101, 127, 201}, {0, 17, 0, 23, 0, 31}), image) == ImageResult::Success);
    CheckBytes(image, {17, 23, 31, 0, 101, 127, 201, 255});
}

TEST_CASE("PNG interlacing fills the final image at source precision", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    for (unsigned bytes : {1u, 2u})
    {
        auto png = Header(9, 9, static_cast<uint8_t>(bytes * 8), 6, true);
        Bytes expected(9 * 9 * 4 * bytes), raw;
        for (size_t i = 0; i < expected.size(); ++i)
            expected[i] = static_cast<uint8_t>(i * 37);
        raw = Scanlines(expected, 9, 9, 4 * bytes, true);
        Chunk(png, "IDAT", Deflate(raw));
        Chunk(png, "IEND", {});
        ImageSharedPtr image;
        REQUIRE(Decode(png, image) == ImageResult::Success);
        if (bytes == 1)
            CheckBytes(image, expected);
        else
        {
            std::vector<uint16_t> words;
            for (size_t i = 0; i < expected.size(); i += 2)
                words.push_back(static_cast<uint16_t>((expected[i] << 8) | expected[i + 1]));
            CheckWords(image, words);
        }
    }
}

TEST_CASE("APNG composites offsets blending and disposal into immutable frames", "[PNG][APNG][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame red{.pixels = {255, 0, 0, 255, 255, 0, 0, 255}};
    for (uint8_t dispose : {0, 1, 2})
    {
        const Frame blue{.width       = 1,
                         .x           = 1,
                         .denominator = 0,
                         .dispose     = dispose,
                         .blend       = 1,
                         .pixels      = {0, 0, 255, 128}};
        const Frame green{.width = 1, .pixels = {0, 255, 0, 255}};
        ImageSharedPtr image;
        REQUIRE(Decode(Animation({red, blue, green}), image) == ImageResult::Success);
        REQUIRE(image->GetSubImageGroupType() == ImageItemType::AnimationFrame);
        REQUIRE(image->GetNumSubImages() == 3);
        CheckBytes(image->GetSubImage(0), red.pixels);
        CheckBytes(image->GetSubImage(1), {255, 0, 0, 255, 127, 0, 128, 255});
        REQUIRE(image->GetSubImage(1)->GetAnimationData().delayMilliseconds == 10);
        const Bytes last = dispose == 0   ? Bytes{0, 255, 0, 255, 127, 0, 128, 255}
                           : dispose == 1 ? Bytes{0, 255, 0, 255, 0, 0, 0, 0}
                                          : Bytes{0, 255, 0, 255, 255, 0, 0, 255};
        CheckBytes(image->GetSubImage(2), last);
    }
}

TEST_CASE("APNG excludes hidden defaults and preserves SOURCE transparency", "[PNG][APNG][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame source{.pixels = {17, 33, 99, 0, 101, 127, 201, 129}};
    for (bool hidden : {false, true})
    {
        ImageSharedPtr image;
        REQUIRE(Decode(Animation({source}, hidden), image) == ImageResult::Success);
        REQUIRE(image->GetNumSubImages() == 1);
        CheckBytes(image->GetSubImage(0), source.pixels);
        REQUIRE(Decode(Animation({source}, hidden), image, ImageLoadFlags::LoadOnlyFirstImage) == ImageResult::Success);
        REQUIRE(image->GetNumSubImages() == 0);
        CheckBytes(image, source.pixels);
    }
    // A hidden default need not have the same rectangle as the first displayed frame.
    const Frame partial{.width = 1, .x = 1, .pixels = {17, 33, 99, 0}};
    ImageSharedPtr image;
    REQUIRE(Decode(Animation({partial}, true), image) == ImageResult::Success);
    CheckBytes(image->GetSubImage(0), {0, 0, 0, 0, 17, 33, 99, 0});
}

TEST_CASE("APNG preserves 16 bit SOURCE and composes OVER at 16 bit precision", "[PNG][APNG][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame source{.pixels = {0x12, 0x34, 0xab, 0xcd, 0, 1, 0, 0, 0xff, 0xff, 0, 0, 0, 0, 0xff, 0xff}};
    const Frame over{.width = 1, .x = 1, .blend = 1, .pixels = {0, 0, 0, 0, 0xff, 0xff, 0x80, 0}};
    ImageSharedPtr image;
    REQUIRE(Decode(Animation({source, over}, false, 16), image) == ImageResult::Success);
    CheckWords(image->GetSubImage(0), {0x1234, 0xabcd, 1, 0, 65535, 0, 0, 65535});
    CheckWords(image->GetSubImage(1), {0x1234, 0xabcd, 1, 0, 32767, 0, 32768, 65535});
}

TEST_CASE("PNG failures preserve the previous output and first-only skips later frames", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    ImageSharedPtr image;
    auto valid = Png(8, 6, {1, 2, 3, 4, 5, 6, 7, 8});
    REQUIRE(Decode(valid, image) == ImageResult::Success);
    const auto previous = image;
    for (size_t size : {size_t{1}, size_t{33}, valid.size() - 1})
    {
        REQUIRE(Decode(Bytes(valid.begin(), valid.begin() + size), image) == ImageResult::FileIsCorrupted);
        REQUIRE(image == previous);
    }
    valid[valid.size() - 1] ^= 1;
    REQUIRE(Decode(valid, image) == ImageResult::FileIsCorrupted);
    REQUIRE(image == previous);
    REQUIRE(Codec()->Decode(nullptr, 1, ImageLoadFlags::None, {}, image) == ImageResult::BadParameters);
    auto oversized = Header(0x7fffffff, 0x7fffffff, 16, 6);
    Chunk(oversized, "IDAT", Deflate({0}));
    REQUIRE(Decode(oversized, image) == ImageResult::FormatNotSupported);
    REQUIRE(image == previous);
    auto png = Animation(
        {Frame{.pixels = {1, 2, 3, 255, 4, 5, 6, 255}}, Frame{.pixels = {7, 8, 9, 255, 10, 11, 12, 255}}});
    png.resize(png.size() - 20);
    REQUIRE(Decode(png, image, ImageLoadFlags::LoadOnlyFirstImage) == ImageResult::Success);
    CheckBytes(image, {1, 2, 3, 255, 4, 5, 6, 255});
    const auto first = image;
    REQUIRE(Decode(png, image) == ImageResult::FileIsCorrupted);
    REQUIRE(image == first);
}

TEST_CASE("PNG encoding respects source row pitch and preserves output on rejection", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    for (const auto format : {TexelFormat::I_R8_G8_B8, TexelFormat::I_R8_G8_B8_A8})
    {
        const size_t pitch = format == TexelFormat::I_R8_G8_B8 ? 8 : 12;
        const size_t row   = format == TexelFormat::I_R8_G8_B8 ? 6 : 8;
        auto item          = std::make_shared<ImageItem>();
        item->descriptor   = {2, 2, static_cast<uint32_t>(pitch), format, format};
        item->data.Allocate(pitch * 2);
        for (size_t i = 0; i < pitch * 2; ++i)
            item->data.data()[i] = static_cast<std::byte>(i * 7);
        auto source = std::make_shared<Image>(item, ImageItemType::Unknown);
        LLUtils::Buffer encoded;
        REQUIRE(Codec()->Encode(source, {}, encoded) == ImageResult::Success);
        ImageSharedPtr image;
        REQUIRE(Codec()->Decode(encoded.data(), encoded.size(), ImageLoadFlags::None, {}, image) ==
                ImageResult::Success);
        for (size_t y = 0; y < 2; ++y)
            REQUIRE(std::memcmp(source->GetBuffer() + y * pitch, image->GetBuffer() + y * row, row) == 0);
        const auto old = encoded.Clone();
        REQUIRE(Codec()->Encode(nullptr, {}, encoded) == ImageResult::BadParameters);
        REQUIRE(encoded.size() == old.size());
        REQUIRE(std::memcmp(encoded.data(), old.data(), old.size()) == 0);
        item->descriptor.texelFormatDecompressed = TexelFormat::I_X8;
        REQUIRE(Codec()->Encode(source, {}, encoded) == ImageResult::FormatNotSupported);
        item->descriptor.texelFormatDecompressed = format;
        item->descriptor.rowPitchInBytes         = 1;
        REQUIRE(Codec()->Encode(source, {}, encoded) == ImageResult::BadParameters);
    }
}

TEST_CASE("PNG loader routes PNG and APNG through the migrated codec", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame frame{.pixels = {17, 33, 99, 0, 101, 127, 201, 129}};
    for (const auto& png : {Png(8, 6, frame.pixels), Animation({frame})})
    {
        ImageLoader loader;
        ImageSharedPtr image;
        REQUIRE(loader.Decode(reinterpret_cast<const std::byte*>(png.data()), png.size(),
                              ImageLoadFlags::LoadOnlyFirstImage, {}, LLUTILS_TEXT("png"),
                              PluginTraverseMode::NoTraverse, image) == ImageResult::Success);
        CheckBytes(image, frame.pixels);
    }
}

TEST_CASE("APNG handles interlaced rectangles and consecutive PREVIOUS disposal", "[PNG][APNG][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame base{.height = 2, .pixels = {255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255}};
    const Frame blue{.width = 1, .height = 2, .x = 1, .dispose = 2, .pixels = {0, 0, 255, 255, 0, 0, 255, 255}};
    const Frame green{.width = 1, .x = 1, .y = 1, .dispose = 2, .blend = 1, .pixels = {0, 255, 0, 128}};
    const Frame transparent{.width = 1, .blend = 1, .pixels = {17, 33, 99, 0}};
    for (bool interlaced : {false, true})
        for (bool hidden : {false, true})
        {
            ImageSharedPtr image;
            REQUIRE(Decode(Animation({base, blue, green, transparent}, hidden, 8, 2, 2, interlaced), image) ==
                    ImageResult::Success);
            CheckBytes(image->GetSubImage(0), base.pixels);
            CheckBytes(image->GetSubImage(1), {255, 0, 0, 255, 0, 0, 255, 255, 255, 0, 0, 255, 0, 0, 255, 255});
            CheckBytes(image->GetSubImage(2), {255, 0, 0, 255, 255, 0, 0, 255, 255, 0, 0, 255, 127, 128, 0, 255});
            CheckBytes(image->GetSubImage(3), base.pixels);
        }
}

TEST_CASE("APNG OVER preserves straight alpha on a transparent canvas", "[PNG][APNG][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame first{.width = 1, .x = 1, .blend = 1, .pixels = {17, 33, 99, 128}};
    const Frame second{.width = 1, .x = 1, .blend = 1, .pixels = {201, 127, 3, 128}};
    ImageSharedPtr image;
    REQUIRE(Decode(Animation({first, second}, true), image) == ImageResult::Success);
    CheckBytes(image->GetSubImage(0), {0, 0, 0, 0, 17, 33, 99, 128});
    CheckBytes(image->GetSubImage(1), {0, 0, 0, 0, 140, 96, 35, 192});
}

TEST_CASE("APNG rejects invalid rectangles and unsupported frame counts", "[PNG][APNG][ImageCompatibility]")
{
    using namespace IMCodec;
    ImageSharedPtr image;
    const Frame valid{.pixels = {1, 2, 3, 255, 4, 5, 6, 255}};
    REQUIRE(Decode(Animation({valid}), image) == ImageResult::Success);
    const auto previous = image;
    const Frame invalid{.width = 1, .x = 2, .pixels = {7, 8, 9, 255}};
    REQUIRE(Decode(Animation({valid, invalid}), image) == ImageResult::FileIsCorrupted);
    REQUIRE(image == previous);
    auto excessive = Header(2, 1, 8, 6);
    Bytes control;
    Big32(control, 65536);
    Big32(control, 0);
    Chunk(excessive, "acTL", control);
    Chunk(excessive, "IDAT", Deflate({0, 1, 2, 3, 255, 4, 5, 6, 255}));
    REQUIRE(Decode(excessive, image) == ImageResult::FormatNotSupported);
    REQUIRE(image == previous);
}

TEST_CASE("PNG retains color declarations separately from decoded samples", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    using ColorInfo = ImageItem::ColorInfo;
    const Bytes pixels{17, 33, 99, 0, 101, 127, 201, 129};
    ImageSharedPtr image;
    REQUIRE(Decode(Png(8, 6, pixels), image) == ImageResult::Success);
    auto info = image->GetImageItem()->colorInfo;
    REQUIRE(info.encoding == ColorInfo::Encoding::Unknown);
    REQUIRE(info.transferFunction == ColorInfo::TransferFunction::Unknown);
    REQUIRE(info.alphaMode == ColorInfo::AlphaMode::Straight);
    REQUIRE_FALSE(info.gamma.has_value());
    REQUIRE_FALSE(info.iccProfile);
    for (uint32_t gamma : {100000u, 50000u})
    {
        auto png = Png(8, 6, pixels);
        Bytes data;
        Big32(data, gamma);
        InsertChunk(png, "gAMA", data);
        REQUIRE(Decode(png, image) == ImageResult::Success);
        info = image->GetImageItem()->colorInfo;
        REQUIRE(info.gamma == gamma / 100000.0);
        REQUIRE(info.encoding == ColorInfo::Encoding::Unknown);
        REQUIRE(info.transferFunction ==
                (gamma == 100000 ? ColorInfo::TransferFunction::Linear : ColorInfo::TransferFunction::Gamma));
        CheckBytes(image, pixels);
    }
    auto srgb = Png(8, 6, pixels, {}, {}, true);
    REQUIRE(Decode(srgb, image) == ImageResult::Success);
    info = image->GetImageItem()->colorInfo;
    REQUIRE(info.encoding == ColorInfo::Encoding::SRGB);
    REQUIRE(info.transferFunction == ColorInfo::TransferFunction::SRGB);
    REQUIRE(info.srgbRenderingIntent == ColorInfo::RenderingIntent::Perceptual);
    REQUIRE(info.gamma.has_value());
    REQUIRE(info.gamma.value() == 1.0);
    CheckBytes(image, pixels);
    auto srgbOnly = Png(8, 6, pixels);
    InsertChunk(srgbOnly, "sRGB", {0});
    REQUIRE(Decode(srgbOnly, image) == ImageResult::Success);
    REQUIRE(image->GetImageItem()->colorInfo.encoding == ColorInfo::Encoding::SRGB);
    REQUIRE_FALSE(image->GetImageItem()->colorInfo.gamma.has_value());
    REQUIRE(Decode(Png(8, 2, {17, 33, 99, 101, 127, 201}), image) == ImageResult::Success);
    REQUIRE(image->GetImageItem()->colorInfo.alphaMode == ColorInfo::AlphaMode::Opaque);
    REQUIRE(Decode(Png(8, 0, {17, 99}, {0, 17}), image) == ImageResult::Success);
    REQUIRE(image->GetImageItem()->colorInfo.alphaMode == ColorInfo::AlphaMode::Straight);
    REQUIRE(Decode(Png(16, 6, {0, 17, 0, 33, 0, 99, 0, 0, 0, 101, 0, 127, 0, 201, 0, 129}), image) ==
            ImageResult::Success);
    REQUIRE(image->GetImageItem()->colorInfo.alphaMode == ColorInfo::AlphaMode::Straight);
    REQUIRE(ImageItem{}.colorInfo.alphaMode == ColorInfo::AlphaMode::Unknown);
}

TEST_CASE("PNG owns ICC bytes and APNG frames share their profile", "[PNG][APNG][ImageCompatibility]")
{
    using namespace IMCodec;
    using ColorInfo    = ImageItem::ColorInfo;
    const auto profile = ICCProfile();
    const Frame frame{.pixels = {17, 33, 99, 0, 101, 127, 201, 129}};
    auto png = Png(8, 6, frame.pixels);
    InsertProfile(png, profile);
    InsertChunk(png, "sRGB", {0});
    Bytes gamma;
    Big32(gamma, 50000);
    InsertChunk(png, "gAMA", gamma);
    ImageSharedPtr image;
    REQUIRE(Decode(png, image) == ImageResult::Success);
    std::fill(png.begin(), png.end(), 0);
    const auto info = image->GetImageItem()->colorInfo;
    REQUIRE(info.encoding == ColorInfo::Encoding::ICCProfile);
    REQUIRE(info.transferFunction == ColorInfo::TransferFunction::Unknown);
    REQUIRE(info.iccProfile);
    REQUIRE(info.gamma == 0.5);
    REQUIRE(info.iccProfile->size() == profile.size());
    REQUIRE(std::memcmp(info.iccProfile->data(), profile.data(), profile.size()) == 0);
    CheckBytes(image, frame.pixels);

    png = Animation({frame, frame});
    InsertProfile(png, profile);
    REQUIRE(Decode(png, image) == ImageResult::Success);
    const auto shared = image->GetImageItem()->colorInfo.iccProfile;
    REQUIRE(shared);
    for (uint16_t index = 0; index < image->GetNumSubImages(); ++index)
    {
        const auto& frameInfo = image->GetSubImage(index)->GetImageItem()->colorInfo;
        REQUIRE(frameInfo.iccProfile == shared);
        REQUIRE(frameInfo.encoding == ColorInfo::Encoding::ICCProfile);
        REQUIRE(frameInfo.alphaMode == ColorInfo::AlphaMode::Straight);
        CheckBytes(image->GetSubImage(index), frame.pixels);
    }
    REQUIRE(Decode(png, image, ImageLoadFlags::LoadOnlyFirstImage) == ImageResult::Success);
    REQUIRE(image->GetImageItem()->colorInfo.iccProfile);
    CheckBytes(image, frame.pixels);
}

TEST_CASE("Image storage operations retain codec color information", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    using ColorInfo  = ImageItem::ColorInfo;
    auto item        = std::make_shared<ImageItem>();
    item->descriptor = {2, 2, 12, TexelFormat::I_R8_G8_B8_A8, TexelFormat::I_R8_G8_B8_A8};
    item->data.Allocate(24);
    std::fill_n(item->data.data(), 24, std::byte{127});
    item->colorInfo.encoding   = ColorInfo::Encoding::ICCProfile;
    item->colorInfo.alphaMode  = ColorInfo::AlphaMode::Straight;
    item->colorInfo.gamma      = 0.5;
    item->colorInfo.iccProfile = std::make_shared<const std::vector<std::byte>>(4, std::byte{42});
    auto source                = std::make_shared<Image>(item, ImageItemType::Unknown);
    const auto rotated = IMUtil::ImageUtil::Transform({IMUtil::Rotate180, IMUtil::AxisAlignedFlip::None}, source);
    const auto packed  = IMUtil::ImageUtil::NormalizePitch(source);
    const auto cropped = IMUtil::ImageUtil::CropImage(packed, {{0, 0}, {1, 1}});
    for (const auto& image : {rotated, packed, cropped})
    {
        REQUIRE(image);
        const auto& info = image->GetImageItem()->colorInfo;
        REQUIRE(info.encoding == item->colorInfo.encoding);
        REQUIRE(info.alphaMode == item->colorInfo.alphaMode);
        REQUIRE(info.gamma == item->colorInfo.gamma);
        REQUIRE(info.iccProfile == item->colorInfo.iccProfile);
    }
}

TEST_CASE("PNG encoding preserves color declarations and source samples", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    using ColorInfo = ImageItem::ColorInfo;
    const Bytes pixels{17, 33, 99, 0, 101, 127, 201, 129};
    auto gammaPng = Png(8, 6, pixels);
    Bytes gamma;
    Big32(gamma, 50000);
    InsertChunk(gammaPng, "gAMA", gamma);
    std::vector<Bytes> files{gammaPng};
    for (uint8_t intent = 0; intent < 4; ++intent)
    {
        auto png = Png(8, 6, pixels);
        InsertChunk(png, "sRGB", {intent});
        files.push_back(std::move(png));
    }
    auto iccPng = gammaPng;
    InsertProfile(iccPng, ICCProfile());
    files.push_back(std::move(iccPng));
    for (const auto& png : files)
    {
        ImageSharedPtr source, decoded;
        REQUIRE(Decode(png, source) == ImageResult::Success);
        const auto& expected = source->GetImageItem()->colorInfo;
        LLUtils::Buffer encoded;
        REQUIRE(Codec()->Encode(source, {}, encoded) == ImageResult::Success);
        REQUIRE(Codec()->Decode(encoded.data(), encoded.size(), ImageLoadFlags::None, {}, decoded) ==
                ImageResult::Success);
        CheckBytes(source, pixels);
        CheckBytes(decoded, pixels);
        const auto& actual = decoded->GetImageItem()->colorInfo;
        REQUIRE(actual.encoding == expected.encoding);
        REQUIRE(actual.transferFunction == expected.transferFunction);
        REQUIRE(actual.gamma == expected.gamma);
        REQUIRE(actual.srgbRenderingIntent == expected.srgbRenderingIntent);
        REQUIRE(actual.alphaMode == expected.alphaMode);
        if (expected.iccProfile)
        {
            REQUIRE(actual.iccProfile);
            REQUIRE(*actual.iccProfile == *expected.iccProfile);
        }
        else
            REQUIRE_FALSE(actual.iccProfile);
    }
}

TEST_CASE("PNG rejects incompatible or invalid color information without replacing output", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    using ColorInfo = ImageItem::ColorInfo;
    ImageSharedPtr source;
    const Bytes pixels{17, 33, 99, 0, 101, 127, 201, 129};
    REQUIRE(Decode(Png(8, 6, pixels), source) == ImageResult::Success);
    LLUtils::Buffer encoded(4);
    std::fill_n(encoded.data(), encoded.size(), std::byte{42});
    auto& info               = source->GetImageItem()->colorInfo;
    const auto original      = info;
    const auto checkRejected = [&](ImageResult result)
    {
        REQUIRE(Codec()->Encode(source, {}, encoded) == result);
        REQUIRE(encoded.size() == 4);
        for (size_t i = 0; i < encoded.size(); ++i)
            REQUIRE(encoded.data()[i] == std::byte{42});
        CheckBytes(source, pixels);
    };
    info.alphaMode = ColorInfo::AlphaMode::Premultiplied;
    checkRejected(ImageResult::FormatNotSupported);
    info = original;
    for (double gamma :
         {0.0, -1.0, 1e-12, 1e20, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        info.gamma = gamma;
        checkRejected(ImageResult::BadParameters);
    }
    info          = original;
    info.encoding = ColorInfo::Encoding::ICCProfile;
    checkRejected(ImageResult::BadParameters);
    info.iccProfile = std::make_shared<const std::vector<std::byte>>(4, std::byte{42});
    checkRejected(ImageResult::BadParameters);
    const auto cmykProfile       = ICCProfile("test1.icc");
    const auto* firstProfileByte = reinterpret_cast<const std::byte*>(cmykProfile.data());
    info.iccProfile = std::make_shared<const std::vector<std::byte>>(firstProfileByte,
                                                                     firstProfileByte + cmykProfile.size());
    checkRejected(ImageResult::FormatNotSupported);
    info                     = original;
    info.encoding            = ColorInfo::Encoding::SRGB;
    info.srgbRenderingIntent = static_cast<ColorInfo::RenderingIntent>(4);
    checkRejected(ImageResult::BadParameters);
    info.srgbRenderingIntent.reset();
    REQUIRE(Codec()->Encode(source, {}, encoded) == ImageResult::Success);
    ImageSharedPtr decoded;
    REQUIRE(Codec()->Decode(encoded.data(), encoded.size(), ImageLoadFlags::None, {}, decoded) == ImageResult::Success);
    REQUIRE(decoded->GetImageItem()->colorInfo.srgbRenderingIntent == ColorInfo::RenderingIntent::Perceptual);
}

TEST_CASE("Channel conversion retains encoding and adjusts alpha interpretation", "[PNG][ImageCompatibility]")
{
    using namespace IMCodec;
    using ColorInfo = ImageItem::ColorInfo;
    auto png        = Png(8, 6, {17, 33, 99, 0, 101, 127, 201, 129});
    InsertProfile(png, ICCProfile());
    ImageSharedPtr source;
    REQUIRE(Decode(png, source) == ImageResult::Success);
    source->GetImageItem()->colorInfo.gamma = 0.5;
    const auto sourceInfo                   = source->GetImageItem()->colorInfo;
    const auto checkEncoding                = [&](const ImageSharedPtr& image)
    {
        REQUIRE(image);
        const auto& info = image->GetImageItem()->colorInfo;
        REQUIRE(info.encoding == sourceInfo.encoding);
        REQUIRE(info.transferFunction == sourceInfo.transferFunction);
        REQUIRE(info.gamma == sourceInfo.gamma);
        REQUIRE(info.iccProfile == sourceInfo.iccProfile);
        REQUIRE(info.srgbRenderingIntent == sourceInfo.srgbRenderingIntent);
    };
    const auto bgra = IMUtil::ImageUtil::Convert(source, TexelFormat::I_B8_G8_R8_A8);
    checkEncoding(bgra);
    CheckBytes(bgra, {99, 33, 17, 0, 201, 127, 101, 129});
    REQUIRE(bgra->GetImageItem()->colorInfo.alphaMode == ColorInfo::AlphaMode::Straight);
    const auto bgr = IMUtil::ImageUtil::Convert(source, TexelFormat::I_B8_G8_R8);
    checkEncoding(bgr);
    CheckBytes(bgr, {99, 33, 17, 201, 127, 101});
    REQUIRE(bgr->GetImageItem()->colorInfo.alphaMode == ColorInfo::AlphaMode::Opaque);
    const auto rgba = IMUtil::ImageUtil::Convert(bgr, TexelFormat::I_R8_G8_B8_A8);
    checkEncoding(rgba);
    CheckBytes(rgba, {17, 33, 99, 255, 101, 127, 201, 255});
    REQUIRE(rgba->GetImageItem()->colorInfo.alphaMode == ColorInfo::AlphaMode::Opaque);

    REQUIRE(Decode(Png(8, 0, {17, 99}), source) == ImageResult::Success);
    source->GetImageItem()->colorInfo.gamma            = 0.5;
    source->GetImageItem()->colorInfo.transferFunction = ColorInfo::TransferFunction::Gamma;
    const auto normalized = IMUtil::ImageUtil::ConvertImageWithNormalization(source, TexelFormat::I_R8_G8_B8_A8, false);
    REQUIRE(normalized);
    REQUIRE_FALSE(normalized->GetImageItem()->colorInfo.gamma.has_value());
    REQUIRE(normalized->GetImageItem()->colorInfo.transferFunction == ColorInfo::TransferFunction::Unknown);
}

TEST_CASE("APNG frame-reader validation rejects malformed frame controls", "[PNG][APNG][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame valid{.pixels = {1, 2, 3, 255, 4, 5, 6, 255}};
    ImageSharedPtr image;
    REQUIRE(Decode(Animation({valid}), image) == ImageResult::Success);
    const auto previous = image;
    const std::array invalidFrames{Frame{.width = 0}, Frame{.height = 0}, Frame{.dispose = 3, .pixels = valid.pixels},
                                   Frame{.blend = 2, .pixels = valid.pixels}};
    for (const auto& invalid : invalidFrames)
    {
        REQUIRE(Decode(Animation({valid, invalid}), image) == ImageResult::FileIsCorrupted);
        REQUIRE(image == previous);
    }
}
