#include <ImageLoader.h>
#include "../../External/ImageCodec/Codecs/CodecGif/Include/CodecGifFactory.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <memory>
#include <span>
#include <vector>

namespace
{
    using Bytes = std::vector<uint8_t>;
    using Color = std::array<uint8_t, 3>;
    constexpr std::array<Color, 4> globalPalette{{{0, 0, 0}, {255, 0, 0}, {0, 255, 0}, {0, 0, 255}}};

    struct Frame
    {
        uint16_t width = 2, height = 1, left = 0, top = 0;
        uint8_t disposal     = 1;
        int transparentIndex = -1;
        uint16_t delay       = 3;
        bool interlaced      = false;
        Bytes indices;
        std::vector<Color> palette;
    };

    void Little16(Bytes& bytes, uint16_t value)
    {
        bytes.push_back(static_cast<uint8_t>(value));
        bytes.push_back(static_cast<uint8_t>(value >> 8));
    }

    void WritePalette(Bytes& bytes, std::span<const Color> palette)
    {
        for (const auto& color : palette)
            bytes.insert(bytes.end(), color.begin(), color.end());
    }

    Bytes Gif(const std::vector<Frame>& frames, uint16_t width = 2, uint16_t height = 1)
    {
        Bytes gif{'G', 'I', 'F', '8', '9', 'a'};
        Little16(gif, width);
        Little16(gif, height);
        gif.insert(gif.end(), {0x81, 0, 0});
        WritePalette(gif, globalPalette);
        for (const auto& frame : frames)
        {
            REQUIRE(frame.indices.size() == size_t{frame.width} * frame.height);
            gif.insert(gif.end(),
                       {0x21, 0xf9, 4, static_cast<uint8_t>((frame.disposal << 2) | (frame.transparentIndex >= 0))});
            Little16(gif, frame.delay);
            gif.insert(gif.end(), {static_cast<uint8_t>(std::max(0, frame.transparentIndex)), 0, 0x2c});
            for (auto value : {frame.left, frame.top, frame.width, frame.height})
                Little16(gif, value);
            uint8_t flags = frame.interlaced ? 0x40 : 0;
            if (!frame.palette.empty())
            {
                REQUIRE((frame.palette.size() == 2 || frame.palette.size() == 4));
                flags |= frame.palette.size() == 2 ? 0x80 : 0x81;
            }
            gif.push_back(flags);
            WritePalette(gif, frame.palette);

            // A clear code before each literal keeps the LZW code width at three
            // bits. These small independent fixtures need no dictionary encoder.
            Bytes compressed;
            unsigned packed = 0, bitCount = 0;
            const auto emit = [&](unsigned code)
            {
                packed |= code << bitCount;
                bitCount += 3;
                while (bitCount >= 8)
                {
                    compressed.push_back(static_cast<uint8_t>(packed));
                    packed >>= 8;
                    bitCount -= 8;
                }
            };
            const auto row = [&](size_t y)
            {
                for (size_t x = 0; x < frame.width; ++x)
                {
                    emit(4);
                    emit(frame.indices[y * frame.width + x]);
                }
            };
            if (frame.interlaced)
            {
                constexpr std::array<size_t, 4> starts{0, 4, 2, 1}, steps{8, 8, 4, 2};
                for (size_t pass = 0; pass < starts.size(); ++pass)
                    for (size_t y = starts[pass]; y < frame.height; y += steps[pass])
                        row(y);
            }
            else
                for (size_t y = 0; y < frame.height; ++y)
                    row(y);
            emit(5);
            if (bitCount)
                compressed.push_back(static_cast<uint8_t>(packed));
            gif.push_back(2);
            for (size_t offset = 0; offset < compressed.size(); offset += 255)
            {
                const auto count = std::min(size_t{255}, compressed.size() - offset);
                gif.push_back(static_cast<uint8_t>(count));
                gif.insert(gif.end(), compressed.begin() + offset, compressed.begin() + offset + count);
            }
            gif.push_back(0);
        }
        gif.push_back(0x3b);
        return gif;
    }

    IMCodec::ImageResult Decode(const Bytes& bytes, IMCodec::ImageSharedPtr& image,
                                IMCodec::ImageLoadFlags flags = IMCodec::ImageLoadFlags::None)
    {
        std::unique_ptr<IMCodec::IImagePlugin> codec(IMCodec::CodecGifFactory::Create());
        return codec->Decode(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size(), flags, {}, image);
    }

    void CheckPixels(const IMCodec::ImageSharedPtr& image, const Bytes& expected)
    {
        REQUIRE(image->GetTexelFormat() == IMCodec::TexelFormat::I_R8_G8_B8_A8);
        REQUIRE(image->GetImageItem()->data.size() == expected.size());
        REQUIRE(std::memcmp(image->GetBuffer(), expected.data(), expected.size()) == 0);
    }
}  // namespace

TEST_CASE("GIF transparency and local palettes retain immutable frame pixels", "[GIF][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame first{.transparentIndex = 0, .indices = {1, 0}};
    const Frame second{.transparentIndex = 0, .indices = {0, 1}, .palette = {{255, 255, 0}, {0, 255, 255}}};
    auto gif = Gif({first, second});
    ImageSharedPtr image;
    REQUIRE(Decode(gif, image) == ImageResult::Success);
    std::fill(gif.begin(), gif.end(), 0);
    REQUIRE(image->GetNumSubImages() == 2);
    REQUIRE(image->GetSubImageGroupType() == ImageItemType::AnimationFrame);
    CheckPixels(image->GetSubImage(0), {255, 0, 0, 255, 0, 0, 0, 0});
    CheckPixels(image->GetSubImage(1), {255, 0, 0, 255, 0, 255, 255, 255});
    REQUIRE(image->GetSubImage(0)->GetAnimationData().delayMilliseconds == 30);
}

TEST_CASE("GIF disposal changes the next canvas without changing displayed frames", "[GIF][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame red{.indices = {1, 1}};
    const Frame green{.width = 1, .indices = {2}};
    for (uint8_t disposal : {0, 1, 2, 3})
    {
        const Frame blue{.width = 1, .left = 1, .disposal = disposal, .indices = {3}};
        ImageSharedPtr image;
        REQUIRE(Decode(Gif({red, blue, green}), image) == ImageResult::Success);
        CheckPixels(image->GetSubImage(0), {255, 0, 0, 255, 255, 0, 0, 255});
        CheckPixels(image->GetSubImage(1), {255, 0, 0, 255, 0, 0, 255, 255});
        const Bytes last = disposal == 2   ? Bytes{0, 255, 0, 255, 0, 0, 0, 255}
                           : disposal == 3 ? Bytes{0, 255, 0, 255, 255, 0, 0, 255}
                                           : Bytes{0, 255, 0, 255, 0, 0, 255, 255};
        CheckPixels(image->GetSubImage(2), last);
    }
}

TEST_CASE("GIF transparent BACKGROUND disposal clears to an alpha canvas", "[GIF][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame red{.transparentIndex = 0, .indices = {1, 1}};
    const Frame blue{.width = 1, .left = 1, .disposal = 2, .transparentIndex = 0, .indices = {3}};
    const Frame green{.width = 1, .transparentIndex = 0, .indices = {2}};
    ImageSharedPtr image;
    REQUIRE(Decode(Gif({red, blue, green}), image) == ImageResult::Success);
    CheckPixels(image->GetSubImage(1), {255, 0, 0, 255, 0, 0, 255, 255});
    CheckPixels(image->GetSubImage(2), {0, 255, 0, 255, 0, 0, 0, 0});
}

TEST_CASE("GIF clips partially and wholly off-screen image rectangles", "[GIF][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame partial{.height = 2, .left = 1, .top = 1, .indices = {1, 2, 3, 1}};
    const Frame outside{.left = 3, .top = 3, .indices = {2, 2}};
    ImageSharedPtr image;
    REQUIRE(Decode(Gif({partial, outside}, 2, 2), image) == ImageResult::Success);
    const Bytes expected{0, 0, 0, 255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 0, 0, 255};
    CheckPixels(image->GetSubImage(0), expected);
    CheckPixels(image->GetSubImage(1), expected);
}

TEST_CASE("GIF interlaced raster is displayed in normal row order", "[GIF][ImageCompatibility]")
{
    using namespace IMCodec;
    Frame frame{.height = 9, .interlaced = true};
    Bytes expected;
    for (size_t y = 0; y < frame.height; ++y)
        for (unsigned x = 0; x < frame.width; ++x)
        {
            const auto index = static_cast<uint8_t>((y + x) % globalPalette.size());
            frame.indices.push_back(index);
            const auto& color = globalPalette[index];
            expected.insert(expected.end(), {color[0], color[1], color[2], 255});
        }
    ImageSharedPtr image;
    REQUIRE(Decode(Gif({frame}, 2, 9), image) == ImageResult::Success);
    REQUIRE(image->GetHeight() == 9);
    CheckPixels(image, expected);
}

TEST_CASE("GIF first-image loading returns an independent standalone image", "[GIF][ImageCompatibility]")
{
    using namespace IMCodec;
    auto gif = Gif({Frame{.indices = {1, 2}}, Frame{.indices = {3, 0}}});
    ImageLoader loader;
    ImageSharedPtr image;
    REQUIRE(loader.Decode(reinterpret_cast<const std::byte*>(gif.data()), gif.size(),
                          ImageLoadFlags::LoadOnlyFirstImage, {}, LLUTILS_TEXT("gif"), PluginTraverseMode::NoTraverse,
                          image) == ImageResult::Success);
    std::fill(gif.begin(), gif.end(), 0);
    REQUIRE(image->GetNumSubImages() == 0);
    REQUIRE(image->GetAnimationData().delayMilliseconds == 30);
    CheckPixels(image, {255, 0, 0, 255, 0, 255, 0, 255});
}

TEST_CASE("GIF malformed input preserves a previous image without publishing partial frames",
          "[GIF][ImageCompatibility]")
{
    using namespace IMCodec;
    const Frame red{.indices = {1, 1}};
    auto gif = Gif({red});
    ImageSharedPtr image;
    REQUIRE(Decode(gif, image) == ImageResult::Success);
    const auto previous = image;
    for (size_t size : {size_t{1}, size_t{13}, gif.size() - 1})
    {
        REQUIRE(Decode(Bytes(gif.begin(), gif.begin() + size), image) == ImageResult::FileIsCorrupted);
        REQUIRE(image == previous);
    }
    const Frame invalid{.indices = {2, 2}, .palette = {{0, 0, 0}, {255, 0, 0}}};
    REQUIRE(Decode(Gif({red, invalid}), image) == ImageResult::FileIsCorrupted);
    REQUIRE(image == previous);
    std::unique_ptr<IImagePlugin> codec(CodecGifFactory::Create());
    REQUIRE(codec->Decode(nullptr, 1, ImageLoadFlags::None, {}, image) == ImageResult::BadParameters);
    REQUIRE(image == previous);
}
