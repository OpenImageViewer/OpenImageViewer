#include <ImageLoader.h>

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <span>
#include <vector>
#include <thread>
#include <atomic>
#include <filesystem>
#include <fstream>

namespace
{
    // Owned-input dispatch must stay absent from the client API.
    template <class T>
    concept ExposesOwnedInputAPI = requires { &T::DecodeOwned; } || requires { &T::PreferOwnedInput; };

    static_assert(!ExposesOwnedInputAPI<IMCodec::IImagePlugin>);
    static_assert(!ExposesOwnedInputAPI<IMCodec::ImageCodec>);
    static_assert(!ExposesOwnedInputAPI<IMCodec::ImageLoader>);

    template <class T>
    concept ExposesInputRetention = requires { &T::Retain; } || requires { &T::AcquireInput; };

    static_assert(!ExposesInputRetention<IMCodec::ImageBuffer>);
    static_assert(!ExposesInputRetention<IMCodec::ImageBufferPool>);

    constexpr void WriteUint32(std::span<std::byte> bytes, std::size_t offset, std::uint32_t value)
    {
        for (std::size_t byteIndex = 0; byteIndex < sizeof(value); ++byteIndex)
            bytes[offset + byteIndex] = static_cast<std::byte>(value >> (byteIndex * 8));
    }

    constexpr auto MakeOnePixelDxt1DDS()
    {
        std::array<std::byte, 136> bytes{};
        bytes[0] = std::byte{'D'};
        bytes[1] = std::byte{'D'};
        bytes[2] = std::byte{'S'};
        bytes[3] = std::byte{' '};

        WriteUint32(bytes, 4, 124);          // DDS_HEADER size
        WriteUint32(bytes, 8, 0x00081007);   // Required fields plus linear size
        WriteUint32(bytes, 12, 1);           // Height
        WriteUint32(bytes, 16, 1);           // Width
        WriteUint32(bytes, 20, 8);           // One DXT1 block
        WriteUint32(bytes, 76, 32);          // DDS_PIXELFORMAT size
        WriteUint32(bytes, 80, 0x4);         // DDPF_FOURCC
        WriteUint32(bytes, 84, 0x31545844);  // DXT1
        WriteUint32(bytes, 108, 0x1000);     // DDSCAPS_TEXTURE

        bytes[128] = std::byte{0x00};  // RGB565 red (0xf800)
        bytes[129] = std::byte{0xf8};
        return bytes;
    }
}  // namespace

TEST_CASE("DDS decoder uses four-byte pixels for partial DXT1 blocks", "[ImageCompatibility][DDS]")
{
    constexpr auto dds = MakeOnePixelDxt1DDS();
    IMCodec::ImageLoader loader;
    IMCodec::ImageSharedPtr image;

    const auto result = loader.Decode(dds.data(), dds.size(), IMCodec::ImageLoadFlags::None, {}, LLUTILS_TEXT("dds"),
                                      IMCodec::PluginTraverseMode::NoTraverse, image);

    REQUIRE(result == IMCodec::ImageResult::Success);
    REQUIRE(image != nullptr);
    REQUIRE(image->GetWidth() == 1);
    REQUIRE(image->GetHeight() == 1);
    REQUIRE(image->GetRowPitchInBytes() == 4);
    REQUIRE(image->GetImageItem()->data.size() == 4);
}

namespace
{
    constexpr uint32_t FourCC(char a, char b, char c, char d)
    {
        return static_cast<uint32_t>(a) | (static_cast<uint32_t>(b) << 8) | (static_cast<uint32_t>(c) << 16) |
               (static_cast<uint32_t>(d) << 24);
    }

    std::vector<std::byte> MakeDDS(uint32_t fourCC, std::span<const uint8_t> payload, uint32_t width = 1,
                                   uint32_t height = 1, uint32_t mips = 1, uint32_t dxgiFormat = 0)
    {
        const size_t headerSize = dxgiFormat ? 148 : 128;
        std::vector<std::byte> bytes(headerSize + payload.size());
        WriteUint32(bytes, 0, FourCC('D', 'D', 'S', ' '));
        WriteUint32(bytes, 4, 124);
        WriteUint32(bytes, 8, 0x000a1007);  // dimensions, mip count and linear size
        WriteUint32(bytes, 12, height);
        WriteUint32(bytes, 16, width);
        WriteUint32(bytes, 20, static_cast<uint32_t>(payload.size()));
        WriteUint32(bytes, 28, mips);
        WriteUint32(bytes, 76, 32);
        WriteUint32(bytes, 80, 4);
        WriteUint32(bytes, 84, dxgiFormat ? FourCC('D', 'X', '1', '0') : fourCC);
        WriteUint32(bytes, 108, mips > 1 ? 0x401008 : 0x1000);
        if (dxgiFormat)
        {
            WriteUint32(bytes, 128, dxgiFormat);
            WriteUint32(bytes, 132, 3);  // DDS_DIMENSION_TEXTURE2D
            WriteUint32(bytes, 140, 1);  // array size
        }
        std::memcpy(bytes.data() + headerSize, payload.data(), payload.size());
        return bytes;
    }

    IMCodec::ImageResult DecodeDDS(std::span<const std::byte> bytes, IMCodec::ImageSharedPtr& image,
                                   IMCodec::ImageLoadFlags flags = IMCodec::ImageLoadFlags::None)
    {
        IMCodec::ImageLoader loader;
        return loader.Decode(bytes.data(), bytes.size(), flags, {}, LLUTILS_TEXT("dds"),
                             IMCodec::PluginTraverseMode::NoTraverse, image);
    }
}  // namespace

TEST_CASE("DDS decoder preserves RGB channel order and block alpha", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    ImageSharedPtr image;
    auto dxt1 = MakeOnePixelDxt1DDS();
    REQUIRE(DecodeDDS(dxt1, image) == ImageResult::Success);
    REQUIRE(image->GetTexelFormat() == TexelFormat::I_R8_G8_B8_A8);
    const std::array<std::byte, 4> red{std::byte{255}, std::byte{0}, std::byte{0}, std::byte{255}};
    REQUIRE(std::memcmp(image->GetBuffer(), red.data(), red.size()) == 0);
    // Decoded pixels must survive destruction or reuse of the caller's DDS bytes.
    dxt1.fill(std::byte{});
    REQUIRE(std::memcmp(image->GetBuffer(), red.data(), red.size()) == 0);

    std::array<uint8_t, 16> block{};
    std::fill_n(block.begin(), 8, 0xaa);
    block[9] = 0xf8;
    REQUIRE(DecodeDDS(MakeDDS(FourCC('D', 'X', 'T', '3'), block), image) == ImageResult::Success);
    REQUIRE(image->GetBuffer()[0] == std::byte{255});
    REQUIRE(image->GetBuffer()[3] == std::byte{170});
    block.fill(0);
    block[0] = 128;
    block[1] = 64;
    block[9] = 0xf8;
    REQUIRE(DecodeDDS(MakeDDS(FourCC('D', 'X', 'T', '5'), block), image) == ImageResult::Success);
    REQUIRE(image->GetBuffer()[3] == std::byte{128});

    std::array<uint8_t, 8> transparent{};
    std::fill(transparent.begin() + 4, transparent.end(), 255);
    REQUIRE(DecodeDDS(MakeDDS(FourCC('D', 'X', 'T', '1'), transparent), image) == ImageResult::Success);
    REQUIRE(image->GetBuffer()[3] == std::byte{0});
}

TEST_CASE("DDS DX10 BC4 through BC7 decode into supported viewer formats", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    ImageSharedPtr image;
    std::array<uint8_t, 16> block{};
    block[0] = 128;
    REQUIRE(DecodeDDS(MakeDDS(0, std::span(block).first(8), 1, 1, 1, 80), image) == ImageResult::Success);  // BC4_UNORM
    REQUIRE(image->GetBuffer()[0] == std::byte{128});
    REQUIRE(image->GetBuffer()[1] == std::byte{128});  // Single-channel BC4 expands as grayscale.
    block[0] = 129;
    block[8] = 127;
    REQUIRE(DecodeDDS(MakeDDS(0, block, 1, 1, 1, 84), image) == ImageResult::Success);  // BC5_SNORM
    REQUIRE(image->GetTexelFormat() == TexelFormat::F_R32_G32_B32);
    std::array<float, 3> channels{};
    std::memcpy(channels.data(), image->GetBuffer(), sizeof(channels));
    REQUIRE(channels[0] == -1.f);
    REQUIRE(channels[1] == 1.f);
    REQUIRE(channels[2] == 0.f);
    block.fill(0);
    REQUIRE(DecodeDDS(MakeDDS(0, block, 1, 1, 1, 95), image) == ImageResult::Success);  // BC6H_UF16 zero block
    REQUIRE(image->GetTexelFormat() == TexelFormat::F_R32_G32_B32);
    REQUIRE(image->GetRowPitchInBytes() == 12);
    block[0] = 0x40;  // BC7 mode 6, zero endpoints
    REQUIRE(DecodeDDS(MakeDDS(0, block, 1, 1, 1, 98), image) == ImageResult::Success);
    REQUIRE(image->GetTexelFormat() == TexelFormat::I_R8_G8_B8_A8);
    REQUIRE(image->GetBuffer()[0] == std::byte{0});
    REQUIRE(image->GetBuffer()[3] == std::byte{0});
}

TEST_CASE("DDS mipmaps and first-image requests preserve the viewer image tree", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    std::array<uint8_t, 24> blocks{};
    blocks[1]      = 0xf8;  // red 4x4
    blocks[8]      = 0xe0;
    blocks[9]      = 0x07;  // green 2x2
    blocks[16]     = 0x1f;  // blue 1x1
    const auto dds = MakeDDS(FourCC('D', 'X', 'T', '1'), blocks, 4, 4, 3);
    ImageSharedPtr image;
    REQUIRE(DecodeDDS(dds, image) == ImageResult::Success);
    REQUIRE(image->GetWidth() == 4);
    REQUIRE(image->GetNumSubImages() == 2);
    REQUIRE(image->GetSubImageGroupType() == ImageItemType::Mipmap);
    REQUIRE(image->GetSubImage(0)->GetWidth() == 2);
    REQUIRE(image->GetSubImage(0)->GetBuffer()[1] == std::byte{255});
    REQUIRE(image->GetSubImage(1)->GetWidth() == 1);
    REQUIRE(image->GetSubImage(1)->GetBuffer()[2] == std::byte{255});
    REQUIRE(DecodeDDS(dds, image, ImageLoadFlags::LoadOnlyFirstImage) == ImageResult::Success);
    REQUIRE(image->GetNumSubImages() == 0);

    auto array = MakeDDS(0, std::span(blocks).first(16), 1, 1, 1, 71);  // BC1 array
    WriteUint32(array, 140, 2);
    REQUIRE(DecodeDDS(array, image) == ImageResult::Success);
    REQUIRE(image->GetNumSubImages() == 0);
    REQUIRE(image->GetBuffer()[0] == std::byte{255});  // first surface
}

TEST_CASE("DDS malformed data does not replace a previously decoded image", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    ImageSharedPtr image;
    auto dds = MakeOnePixelDxt1DDS();
    REQUIRE(DecodeDDS(dds, image) == ImageResult::Success);
    const auto previous = image;
    REQUIRE(DecodeDDS(std::span(dds).first(127), image) == ImageResult::FileIsCorrupted);
    REQUIRE(image == previous);
    REQUIRE(DecodeDDS(std::span(dds).first(dds.size() - 1), image) == ImageResult::FileIsCorrupted);
    REQUIRE(image == previous);
    WriteUint32(dds, 16, 0xffffffff);
    REQUIRE(DecodeDDS(dds, image) != ImageResult::Success);
    REQUIRE(image == previous);
}

TEST_CASE("DDS uncompressed BGRA and partial block rows retain their pitches", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    std::array<uint8_t, 24> pixels{};
    for (size_t i = 0; i < pixels.size(); i += 4)
    {
        pixels[i]     = 17;
        pixels[i + 1] = 33;
        pixels[i + 2] = 65;
        pixels[i + 3] = 255;
    }
    auto dds = MakeDDS(0, pixels, 3, 2, 1, 87);  // B8G8R8A8_UNORM
    ImageSharedPtr image;
    REQUIRE(DecodeDDS(dds, image) == ImageResult::Success);
    REQUIRE(image->GetTexelFormat() == TexelFormat::I_B8_G8_R8_A8);
    REQUIRE(image->GetRowPitchInBytes() == 12);
    REQUIRE(std::memcmp(image->GetBuffer(), pixels.data(), pixels.size()) == 0);
    std::fill(dds.begin(), dds.end(), std::byte{});
    REQUIRE(std::memcmp(image->GetBuffer(), pixels.data(), pixels.size()) == 0);
    std::array<uint8_t, 32> blocks{};
    for (size_t i = 1; i < blocks.size(); i += 8)
        blocks[i] = 0xf8;
    dds = MakeDDS(FourCC('D', 'X', 'T', '1'), blocks, 5, 7);
    REQUIRE(DecodeDDS(dds, image) == ImageResult::Success);
    REQUIRE(image->GetRowPitchInBytes() == 20);
    REQUIRE(image->GetImageItem()->data.size() == 140);
    REQUIRE(image->GetBuffer()[136] == std::byte{255});
    REQUIRE(image->GetBuffer()[139] == std::byte{255});
}

TEST_CASE("DDS temporary AYUV conversion preserves colors and alpha", "[ImageCompatibility][DDS][AYUV]")
{
    using namespace IMCodec;
    // AYUV bytes are V, U, Y, A. Include out-of-range chroma and transparent colored pixels.
    constexpr std::array<uint8_t, 20> inputRow{128, 128, 16,  255, 128, 128, 235, 255, 240, 90,
                                               81,  128, 110, 240, 41,  64,  255, 0,   0,   0};
    constexpr std::array<uint8_t, 20> expected{0, 0,   0, 255, 255, 255, 255, 255, 255, 0,
                                               0, 128, 0, 0,   255, 64,  184, 0,   0,   0};
    std::vector<uint8_t> pixels;
    for (size_t row = 0; row < 4; ++row)
        pixels.insert(pixels.end(), inputRow.begin(), inputRow.end());
    auto dds = MakeDDS(0, pixels, 5, 4, 1, 100);  // DXGI_FORMAT_AYUV
    WriteUint32(dds, 8, 0x0002100f);
    WriteUint32(dds, 20, 20);
    WriteUint32(dds, 24, 1);
    ImageSharedPtr image;
    for (const auto flags : {ImageLoadFlags::None, ImageLoadFlags::LoadOnlyFirstImage})
    {
        REQUIRE(DecodeDDS(dds, image, flags) == ImageResult::Success);
        REQUIRE(image->GetTexelFormat() == TexelFormat::I_R8_G8_B8_A8);
        REQUIRE(image->GetWidth() == 5);
        REQUIRE(image->GetHeight() == 4);
        REQUIRE(image->GetRowPitchInBytes() == expected.size());
        REQUIRE(image->GetSizeInMemory() == pixels.size());
        for (size_t row = 0; row < 4; ++row)
            REQUIRE(std::memcmp(image->GetBuffer() + row * expected.size(), expected.data(), expected.size()) == 0);
    }
    const auto previous = image;
    REQUIRE(DecodeDDS(std::span(dds).first(dds.size() - 1), image) == ImageResult::FileIsCorrupted);
    REQUIRE(image == previous);
    std::fill(dds.begin(), dds.end(), std::byte{});
    REQUIRE(std::memcmp(image->GetBuffer(), expected.data(), expected.size()) == 0);
}

TEST_CASE("DDS temporary YUV conversion handles packed and planar chroma", "[ImageCompatibility][DDS][YUV]")
{
    using namespace IMCodec;
    // Each row alternates black and white. Subsampled chroma is neutral.
    for (const uint32_t format : {103u, 107u, 110u})  // NV12, YUY2, NV11
    {
        INFO("DXGI format " << format);
        const std::array<uint8_t, 16> planar{16,  235, 16,  235, 16,  235, 16,  235,
                                             128, 128, 128, 128, 128, 128, 128, 128};
        const std::array<uint8_t, 16> packed{16, 128, 235, 128, 16, 128, 235, 128,
                                             16, 128, 235, 128, 16, 128, 235, 128};
        const auto payload = format == 107 ? std::span(packed) : std::span(planar);
        const auto dds     = MakeDDS(0, payload.first(format == 103 ? 12 : 16), 4, 2, 1, format);
        ImageSharedPtr image;
        REQUIRE(DecodeDDS(dds, image) == ImageResult::Success);
        REQUIRE(image->GetTexelFormat() == TexelFormat::I_R8_G8_B8_A8);
        REQUIRE(image->GetRowPitchInBytes() == 16);
        REQUIRE(image->GetSizeInMemory() == 32);
        for (size_t pixel = 0; pixel < 8; ++pixel)
        {
            for (size_t channel = 0; channel < 3; ++channel)
                REQUIRE(image->GetBuffer()[pixel * 4 + channel] == static_cast<std::byte>(pixel % 2 ? 255 : 0));
            REQUIRE(image->GetBuffer()[pixel * 4 + 3] == std::byte{255});
        }
        const auto previous = image;
        REQUIRE(DecodeDDS(std::span(dds).first(dds.size() - 1), image) == ImageResult::FileIsCorrupted);
        REQUIRE(image == previous);
    }
}

TEST_CASE("DDS temporary YUV conversion accepts high-bit-depth formats", "[ImageCompatibility][DDS][YUV]")
{
    using namespace IMCodec;
    for (const uint32_t format : {101u, 102u, 104u, 105u, 108u, 109u})  // Y410, Y416, P010, P016, Y210, Y216
    {
        INFO("DXGI format " << format);
        std::vector<uint8_t> pixels;
        if (format == 101)
        {
            pixels.resize(16);
            // Neutral black, with 2/3 alpha in Y410's two-bit alpha channel.
            for (size_t offset = 0; offset < pixels.size(); offset += 4)
                WriteUint32(std::as_writable_bytes(std::span(pixels)), offset,
                            512u | (64u << 10) | (512u << 20) | (2u << 30));
        }
        else
        {
            std::vector<uint16_t> words;
            if (format == 102)
                words = {32768, 4096, 32768, 32768, 32768, 4096, 32768, 32768,
                         32768, 4096, 32768, 32768, 32768, 4096, 32768, 32768};
            else if (format == 104 || format == 105)
                words = {4096, 4096, 4096, 4096, 32768, 32768};
            else
                words = {4096, 32768, 4096, 32768, 4096, 32768, 4096, 32768};
            // DDS words are little-endian regardless of the test host.
            for (const auto word : words)
            {
                pixels.push_back(static_cast<uint8_t>(word));
                pixels.push_back(static_cast<uint8_t>(word >> 8));
            }
        }
        ImageSharedPtr image;
        REQUIRE(DecodeDDS(MakeDDS(0, pixels, 2, 2, 1, format), image) == ImageResult::Success);
        REQUIRE(image->GetTexelFormat() == TexelFormat::I_R8_G8_B8_A8);
        REQUIRE(image->GetRowPitchInBytes() == 8);
        REQUIRE(image->GetSizeInMemory() == 16);
        for (size_t pixel = 0; pixel < 4; ++pixel)
        {
            for (size_t channel = 0; channel < 3; ++channel)
                REQUIRE(image->GetBuffer()[pixel * 4 + channel] == std::byte{0});
            const auto alpha = static_cast<std::byte>(format == 101 ? 170 : format == 102 ? 128 : 255);
            REQUIRE(image->GetBuffer()[pixel * 4 + 3] == alpha);
        }
    }
}

TEST_CASE("DDS YUV mipmaps own their converted pixels", "[ImageCompatibility][DDS][YUV]")
{
    using namespace IMCodec;
    std::array<uint8_t, 20> pixels{};  // AYUV 2x2 and 1x1
    for (size_t offset = 0; offset < pixels.size(); offset += 4)
    {
        pixels[offset] = pixels[offset + 1] = 128;
        pixels[offset + 2]                  = offset < 16 ? 16 : 235;
        pixels[offset + 3]                  = 255;
    }
    const auto dds                                = MakeDDS(0, pixels, 2, 2, 2, 100);
    auto input                                    = std::make_shared<LLUtils::Buffer>(dds.data(), dds.size());
    std::weak_ptr<const LLUtils::Buffer> lifetime = input;
    ImageLoader loader;
    ImageSharedPtr image;
    REQUIRE(loader.Decode(input->data(), input->size(), ImageLoadFlags::None, {}, LLUTILS_TEXT("dds"),
                          PluginTraverseMode::NoTraverse, image) == ImageResult::Success);
    input.reset();
    REQUIRE(lifetime.expired());
    REQUIRE(image->GetNumSubImages() == 1);
    REQUIRE(image->GetBuffer()[0] == std::byte{0});
    const auto mip = image->GetSubImage(0);
    REQUIRE(mip->GetTexelFormat() == TexelFormat::I_R8_G8_B8_A8);
    REQUIRE(mip->GetRowPitchInBytes() == 4);
    REQUIRE(mip->GetBuffer()[0] == std::byte{255});
    REQUIRE(DecodeDDS(dds, image, ImageLoadFlags::LoadOnlyFirstImage) == ImageResult::Success);
    REQUIRE(image->GetNumSubImages() == 0);
    const auto previous = image;
    const std::array<uint8_t, 6> opaque{};
    REQUIRE(DecodeDDS(MakeDDS(0, opaque, 2, 2, 1, 106), image) == ImageResult::FormatNotSupported);
    REQUIRE(image == previous);
}

TEST_CASE("DDS first-image requests stop at the base level for arrays and cubes", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    for (const bool cube : {false, true})
    {
        std::vector<uint8_t> blocks((cube ? 6 : 2) * 16);
        blocks[1] = 0xf8;
        blocks[8] = 0xe0;
        blocks[9] = 0x07;
        auto dds  = MakeDDS(0, blocks, 4, 4, 2, 71);
        WriteUint32(dds, 136, cube ? 4 : 0);
        WriteUint32(dds, 140, cube ? 1 : 2);
        ImageSharedPtr image;
        REQUIRE(DecodeDDS(dds, image) == ImageResult::Success);
        REQUIRE(image->GetNumSubImages() == 1);
        REQUIRE(DecodeDDS(dds, image, ImageLoadFlags::LoadOnlyFirstImage) == ImageResult::Success);
        REQUIRE(image->GetNumSubImages() == 0);
        REQUIRE(image->GetBuffer()[0] == std::byte{255});
    }
}

TEST_CASE("DDS legacy expansion falls back to an owning load", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    std::array<uint8_t, 18> pixels{};
    for (size_t i = 0; i < pixels.size(); i += 3)
    {
        pixels[i]     = 17;
        pixels[i + 1] = 33;
        pixels[i + 2] = 65;
    }
    auto dds = MakeDDS(0, pixels, 3, 2);
    WriteUint32(dds, 8, 0x100f);
    WriteUint32(dds, 20, 9);
    WriteUint32(dds, 80, 0x40);
    WriteUint32(dds, 88, 24);
    WriteUint32(dds, 92, 0xff0000);
    WriteUint32(dds, 96, 0xff00);
    WriteUint32(dds, 100, 0xff);
    ImageSharedPtr image;
    REQUIRE(DecodeDDS(dds, image) == ImageResult::Success);
    REQUIRE(image->GetRowPitchInBytes() == 12);
    REQUIRE(image->GetImageItem()->data.size() == 24);
    std::fill(dds.begin(), dds.end(), std::byte{});
    for (size_t i = 0; i < 24; i += 4)
    {
        REQUIRE(image->GetBuffer()[i] == std::byte{65});
        REQUIRE(image->GetBuffer()[i + 1] == std::byte{33});
        REQUIRE(image->GetBuffer()[i + 2] == std::byte{17});
        REQUIRE(image->GetBuffer()[i + 3] == std::byte{255});
    }
}

TEST_CASE("DDS native pixels and every mip remain independent of caller input", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    std::array<uint8_t, 84> pixels{};  // 4x4, 2x2, 1x1 RGBA
    for (size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = static_cast<uint8_t>(i);
    const auto bytes                              = MakeDDS(0, pixels, 4, 4, 3, 28);
    auto input                                    = std::make_shared<LLUtils::Buffer>(bytes.data(), bytes.size());
    std::weak_ptr<const LLUtils::Buffer> lifetime = input;
    const auto* payload                           = input->data() + 148;
    ImageSharedPtr image;
    {
        ImageLoader loader;
        REQUIRE(loader.Decode(input->data(), input->size(), ImageLoadFlags::None, {}, LLUTILS_TEXT("dds"),
                              PluginTraverseMode::NoTraverse, image) == ImageResult::Success);
    }
    REQUIRE(image->GetBuffer() != payload);
    REQUIRE(image->GetSubImage(0)->GetBuffer() != payload + 64);
    auto tail = image->GetSubImage(1);
    REQUIRE(tail->GetBuffer() != payload + 80);
    REQUIRE(std::memcmp(image->GetBuffer(), payload, 64) == 0);
    REQUIRE(std::memcmp(image->GetSubImage(0)->GetBuffer(), payload + 64, 16) == 0);
    auto clone = image->GetImageItem()->data;
    REQUIRE(std::as_const(clone).data() != image->GetBuffer());
    REQUIRE(std::memcmp(std::as_const(clone).data(), image->GetBuffer(), 64) == 0);
    const auto* originalPixels = image->GetBuffer();
    input->data()[148]         = std::byte{17};
    REQUIRE(image->GetBuffer()[0] == std::byte{0});
    auto* writable = image->GetImageItem()->data.data();
    REQUIRE(writable == originalPixels);
    writable[0] = std::byte{255};
    REQUIRE(payload[0] == std::byte{17});
    REQUIRE(image->GetBuffer()[0] == std::byte{255});
    REQUIRE(std::as_const(clone).data()[0] == std::byte{0});
    input.reset();
    REQUIRE(lifetime.expired());
    REQUIRE(image->GetSubImage(0)->GetBuffer()[0] == std::byte{64});
    image.reset();
    REQUIRE(tail->GetBuffer()[0] == std::byte{80});
}

TEST_CASE("Image buffers recycle only after the final image owner and preserve move semantics", "[DDS]")
{
    using namespace IMCodec;
    auto pool  = std::make_shared<ImageBufferPool>(1024);
    auto first = std::make_shared<ImageItem>();
    first->data.Allocate(1024, pool);
    auto* address    = first->data.data();
    address[0]       = std::byte{42};
    auto secondOwner = first;
    first.reset();
    ImageBuffer other;
    other.Allocate(1024, pool);
    REQUIRE(other.data() != address);
    REQUIRE(secondOwner->data.data()[0] == std::byte{42});
    secondOwner.reset();
    ImageBuffer reused;
    reused.Allocate(1024, pool);
    REQUIRE(reused.data() == address);
    REQUIRE(reused.data()[0] == std::byte{42});
    ImageBuffer moved(std::move(reused));
    REQUIRE(moved.data() == address);
    REQUIRE(reused.size() == 0);
    reused = std::move(moved);
    REQUIRE(reused.data() == address);
    REQUIRE(moved.size() == 0);
    // A pool can disappear from the codec while a live buffer still owns its state.
    std::weak_ptr<ImageBufferPool> lifetime = pool;
    pool.reset();
    REQUIRE_FALSE(lifetime.expired());
    reused.Free();
    other.Free();
    REQUIRE(lifetime.expired());
}

TEST_CASE("Image buffer pool bounds idle bytes and supports concurrent owners", "[DDS]")
{
    using namespace IMCodec;
    auto pool = std::make_shared<ImageBufferPool>(128);
    ImageBuffer a, b;
    a.Allocate(128, pool);
    b.Allocate(64, pool);
    auto* first = a.data();
    a.Free();  // fills the budget
    b.Free();  // cannot displace the retained 128-byte buffer
    a.Allocate(128, pool);
    REQUIRE(a.data() == first);
    std::atomic<bool> valid{true};
    std::vector<std::jthread> threads;
    for (int thread = 0; thread < 4; ++thread)
        threads.emplace_back(
            [pool, thread, &valid]
            {
                for (size_t i = 0; i < 1000; ++i)
                {
                    ImageBuffer buffer;
                    buffer.Allocate(64, pool);
                    std::fill_n(buffer.data(), 64, static_cast<std::byte>(thread));
                    std::this_thread::yield();
                    if (!std::all_of(std::as_const(buffer).data(), std::as_const(buffer).data() + 64,
                                     [thread](std::byte value) { return value == static_cast<std::byte>(thread); }))
                        valid = false;
                }
            });
    threads.clear();
    REQUIRE(valid);
}

TEST_CASE("DDS file pixels survive file truncation and deletion", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    constexpr size_t baseBytes = 512 * 512 * 4;
    constexpr size_t mipBytes  = 256 * 256 * 4;
    std::vector<uint8_t> pixels(baseBytes + mipBytes);
    for (size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = static_cast<uint8_t>(i * 17);
    auto bytes = MakeDDS(0, pixels, 512, 512, 2, 28);
    WriteUint32(bytes, 8, 0x2100f);  // Canonical uncompressed mip chain with row pitch.
    WriteUint32(bytes, 20, 512 * 4);
    const auto path = std::filesystem::temp_directory_path() / "oiv-dds-independent-lifetime.dds";
    {
        std::ofstream file(path, std::ios::binary);
        file.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
    ImageLoader loader;
    ImageSharedPtr image;
    const auto name = LLUtils::StringUtility::ConvertString<string_type>(path.native());
    REQUIRE(loader.Decode(name, ImageLoadFlags::None, {}, PluginTraverseMode::NoTraverse, image) ==
            ImageResult::Success);
    {
        std::ofstream truncate(path, std::ios::binary | std::ios::trunc);
    }
    std::filesystem::remove(path);
    REQUIRE(std::memcmp(image->GetBuffer(), pixels.data(), baseBytes) == 0);
    REQUIRE(std::memcmp(image->GetSubImage(0)->GetBuffer(), pixels.data() + baseBytes, mipBytes) == 0);
    const auto* ownedPixels = image->GetBuffer();
    // The disabled retention path must not trigger a detach on writable access.
    REQUIRE(image->GetImageItem()->data.data() == ownedPixels);
    REQUIRE(std::memcmp(image->GetBuffer(), pixels.data(), baseBytes) == 0);
}

TEST_CASE("DDS legacy packed conversion preserves alpha across a full mip chain", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    for (const uint32_t format : {86u, 115u})  // B5G5R5A1 and B4G4R4A4
    {
        std::array<uint8_t, 42> pixels{};  // 4x4, 2x2, 1x1
        std::fill(pixels.begin(), pixels.end(), 255);
        const auto bytes = MakeDDS(0, pixels, 4, 4, 3, format);
        ImageSharedPtr image;
        REQUIRE(DecodeDDS(bytes, image) == ImageResult::Success);
        REQUIRE(image->GetNumSubImages() == 2);
        for (uint16_t mip = 0; mip < 3; ++mip)
        {
            auto level = mip ? image->GetSubImage(mip - 1) : image;
            REQUIRE(level->GetTexelFormat() == TexelFormat::I_R8_G8_B8_A8);
            REQUIRE(std::all_of(level->GetBuffer(), level->GetBuffer() + level->GetSizeInMemory(),
                                [](std::byte value) { return value == std::byte{255}; }));
        }
    }
}

TEST_CASE("Empty image buffers copy and mutate without zero-sized aligned allocation", "[DDS]")
{
    using namespace IMCodec;
    ImageBuffer empty;
    ImageBuffer copy = empty;
    REQUIRE(copy.size() == 0);
    REQUIRE(copy.data() == nullptr);
    copy.Allocate(0);
    REQUIRE(copy.data() == nullptr);
    empty.Read(nullptr, 0, 0);
    empty.Write(nullptr, 0, 0);
    REQUIRE(empty.data() == nullptr);
    REQUIRE(ImageBufferPool(0).Acquire(0).data() == nullptr);
}

TEST_CASE("DDS large parallel strips preserve partial block rows", "[ImageCompatibility][DDS]")
{
    using namespace IMCodec;
    constexpr uint32_t width = 2053, height = 2057;
    const size_t blocks = ((width + 3) / 4) * ((height + 3) / 4);
    for (const bool signedBC5 : {false, true})
    {
        const size_t blockBytes = signedBC5 ? 16 : 8;
        std::vector<uint8_t> pixels(blocks * blockBytes);
        for (size_t i = 0; i < pixels.size(); i += blockBytes)
        {
            if (signedBC5)
            {
                pixels[i]     = 129;
                pixels[i + 8] = 127;
            }
            else
                pixels[i + 1] = 0xf8;
        }
        ImageSharedPtr image;
        REQUIRE(DecodeDDS(MakeDDS(0, pixels, width, height, 1, signedBC5 ? 84 : 71), image) == ImageResult::Success);
        const std::array<float, 3> expectedFloat{-1.f, 1.f, 0.f};
        const std::array<uint8_t, 4> expectedByte{255, 0, 0, 255};
        const auto* expected = signedBC5 ? reinterpret_cast<const void*>(expectedFloat.data()) : expectedByte.data();
        const size_t stride  = signedBC5 ? sizeof(expectedFloat) : sizeof(expectedByte);
        bool equal           = true;
        for (size_t i = 0; i < size_t(width) * height && equal; ++i)
            equal = std::memcmp(image->GetBuffer() + i * stride, expected, stride) == 0;
        REQUIRE(equal);
    }
}
