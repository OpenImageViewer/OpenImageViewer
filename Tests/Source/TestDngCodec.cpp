#include <ImageLoader.h>
#include <ImageUtil/ImageUtil.h>
#include "../../External/ImageCodec/Codecs/CodecTiff/Include/CodecTiffFactory.h"
#include <catch2/catch_test_macros.hpp>
#include <turbojpeg.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <chrono>
#include <cstring>
#if defined(_WIN32)
    #include "../../Clients/OIViewer/SrcPlatform/Win32/WlxPreview.h"
#endif

namespace
{
    using Bytes = std::vector<std::byte>;
    struct Field
    {
        uint16_t tag;
        uint16_t type;
        uint32_t count;
        Bytes data;
    };

    void Put(Bytes& bytes, size_t offset, uint32_t value, size_t size = 4)
    {
        for (size_t i = 0; i < size; ++i)
            bytes.at(offset + i) = static_cast<std::byte>((value >> (i * 8)) & 255);
    }

    uint32_t Get(const Bytes& bytes, size_t offset, size_t size = 4)
    {
        uint32_t value = 0;
        for (size_t i = 0; i < size; ++i)
            value |= std::to_integer<uint32_t>(bytes.at(offset + i)) << (i * 8);
        return value;
    }

    Field Integers(uint16_t tag, uint16_t type, std::initializer_list<uint32_t> values)
    {
        const size_t size = type == 3 ? 2 : 4;
        Field field{tag, type, static_cast<uint32_t>(values.size()), Bytes(values.size() * size)};
        size_t offset = 0;
        for (uint32_t value : values)
        {
            Put(field.data, offset, value, size);
            offset += size;
        }
        return field;
    }

    Field Rationals(uint16_t tag, std::initializer_list<int32_t> numerators, uint32_t denominator)
    {
        Field field{tag, 10, static_cast<uint32_t>(numerators.size()), Bytes(numerators.size() * 8)};
        size_t offset = 0;
        for (int32_t value : numerators)
        {
            Put(field.data, offset, static_cast<uint32_t>(value));
            Put(field.data, offset + 4, denominator);
            offset += 8;
        }
        return field;
    }

    uint32_t Directory(Bytes& bytes, std::vector<Field> fields)
    {
        std::ranges::sort(fields, {}, &Field::tag);
        if (bytes.size() % 2)
            bytes.push_back(std::byte{});
        const auto directory = static_cast<uint32_t>(bytes.size());
        bytes.resize(bytes.size() + 6 + fields.size() * 12);
        Put(bytes, directory, static_cast<uint32_t>(fields.size()), 2);
        for (size_t i = 0; i < fields.size(); ++i)
        {
            const auto& field  = fields[i];
            const size_t entry = directory + 2 + i * 12;
            Put(bytes, entry, field.tag, 2);
            Put(bytes, entry + 2, field.type, 2);
            Put(bytes, entry + 4, field.count);
            if (field.data.size() <= 4)
                std::copy(field.data.begin(), field.data.end(), bytes.begin() + entry + 8);
            else
            {
                Put(bytes, entry + 8, static_cast<uint32_t>(bytes.size()));
                bytes.insert(bytes.end(), field.data.begin(), field.data.end());
                if (bytes.size() % 2)
                    bytes.push_back(std::byte{});
            }
        }
        return directory;
    }

    size_t Entry(const Bytes& bytes, uint32_t directory, uint16_t tag)
    {
        size_t result = 0;
        for (uint32_t i = 0; i < Get(bytes, directory, 2); ++i)
        {
            const size_t entry = directory + 2 + i * 12;
            if (Get(bytes, entry, 2) == tag)
                result = entry;
        }
        REQUIRE(result != 0);
        return result;
    }

    std::vector<unsigned char> LosslessTile(int originX)
    {
        std::array<unsigned char, 16 * 16 * 3> pixels{};
        for (int y = 0; y < 16; ++y)
            for (int x = 0; x < 16; ++x)
            {
                pixels[(y * 16 + x) * 3]     = static_cast<unsigned char>((originX + x) * 7 + y);
                pixels[(y * 16 + x) * 3 + 1] = 64;
                pixels[(y * 16 + x) * 3 + 2] = 128;
            }
        const std::unique_ptr<void, decltype(&tj3Destroy)> jpeg(tj3Init(TJINIT_COMPRESS), tj3Destroy);
        REQUIRE(jpeg);
        REQUIRE(tj3Set(jpeg.get(), TJPARAM_LOSSLESS, 1) == 0);
        REQUIRE(tj3Set(jpeg.get(), TJPARAM_COLORSPACE, TJCS_RGB) == 0);
        unsigned char* buffer = nullptr;
        size_t size           = 0;
        const int status      = tj3Compress8(jpeg.get(), pixels.data(), 16, 0, 16, TJPF_RGB, &buffer, &size);
        const std::unique_ptr<unsigned char, decltype(&tj3Free)> owner(buffer, tj3Free);
        INFO(tj3GetErrorStr(jpeg.get()));
        REQUIRE(status == 0);
        return {buffer, buffer + size};
    }

    Bytes MakeDng(uint32_t width = 17, uint32_t height = 9)
    {
        Bytes bytes{std::byte{'I'}, std::byte{'I'}, std::byte{42}, std::byte{},
                    std::byte{8},   std::byte{},    std::byte{},   std::byte{}};
        const auto root = Directory(
            bytes, {Integers(254, 4, {1}), Integers(256, 4, {1}), Integers(257, 4, {1}), Integers(258, 3, {8, 8, 8}),
                    Integers(259, 3, {1}), Integers(262, 3, {2}), Integers(273, 4, {0}), Integers(277, 3, {3}),
                    Integers(278, 4, {1}), Integers(279, 4, {3}), Integers(284, 3, {1}), Integers(330, 4, {0}),
                    Field{50706, 1, 4, {std::byte{1}, std::byte{7}, std::byte{}, std::byte{}}},
                    Field{50708, 2, 5, {std::byte{'J'}, std::byte{'P'}, std::byte{'E'}, std::byte{'G'}, std::byte{}}},
                    Rationals(50721, {31336, -16168, -4907, -9787, 19161, 335, 721, -2291, 14054}, 10000),
                    Rationals(50727, {1, 1, 1}, 1), Rationals(50729, {3457, 3585}, 10000), Rationals(50730, {0}, 1)});
        REQUIRE(root == 8);
        Put(bytes, Entry(bytes, root, 273) + 8, static_cast<uint32_t>(bytes.size()));
        bytes.insert(bytes.end(), {std::byte{255}, std::byte{}, std::byte{}});
        const uint32_t tilesAcross = (width + 15) / 16;
        const uint32_t tileCount   = tilesAcross * ((height + 15) / 16);
        std::vector<uint32_t> offsets(tileCount), sizes(tileCount);
        for (size_t i = 0; i < tileCount; ++i)
        {
            const auto tile = LosslessTile(static_cast<int>(i % tilesAcross) * 16);
            offsets[i]      = static_cast<uint32_t>(bytes.size());
            sizes[i]        = static_cast<uint32_t>(tile.size());
            for (unsigned char value : tile)
                bytes.push_back(static_cast<std::byte>(value));
        }
        Field table{50712, 3, 256, Bytes(512)};
        for (size_t i = 0; i < 256; ++i)
        {
            const double encoded = static_cast<double>(i) / 255;
            const double linear  = encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
            Put(table.data, i * 2, static_cast<uint16_t>(std::lround(linear * 65535)), 2);
        }
        Field tileOffsets{324, 4, tileCount, Bytes(tileCount * 4)};
        Field tileSizes{325, 4, tileCount, Bytes(tileCount * 4)};
        for (size_t i = 0; i < tileCount; ++i)
        {
            Put(tileOffsets.data, i * 4, offsets[i]);
            Put(tileSizes.data, i * 4, sizes[i]);
        }
        auto black         = Rationals(50714, {0, 0, 0}, 1);
        black.type         = 5;
        auto scale         = Rationals(50718, {1, 1}, 1);
        scale.type         = 5;
        const auto primary = Directory(
            bytes, {Integers(254, 4, {0}), Integers(256, 4, {width}), Integers(257, 4, {height}),
                    Integers(258, 3, {8, 8, 8}), Integers(259, 3, {7}), Integers(262, 3, {34892}),
                    Integers(277, 3, {3}), Integers(284, 3, {1}), Integers(322, 4, {16}), Integers(323, 4, {16}),
                    tileOffsets, tileSizes, table, black, Integers(50717, 4, {65535, 65535, 65535}), scale});
        Put(bytes, Entry(bytes, root, 330) + 8, primary);
        return bytes;
    }

    IMCodec::ImageResult DecodeDng(const Bytes& bytes, IMCodec::ImageSharedPtr& image)
    {
        const std::unique_ptr<IMCodec::IImagePlugin> codec(IMCodec::CodecTiffFactory::Create());
        return codec->Decode(bytes.data(), bytes.size(), IMCodec::ImageLoadFlags::None, {}, image);
    }
}  // namespace

TEST_CASE("RGB DNG retains stored texels and owns its linearization table", "[DNG]")
{
    using namespace IMCodec;
    auto bytes = MakeDng();
    ImageSharedPtr image;
    REQUIRE(DecodeDng(bytes, image) == ImageResult::Success);
    REQUIRE(image);
    REQUIRE(image->GetWidth() == 17);
    REQUIRE(image->GetHeight() == 9);
    REQUIRE(image->GetNumSubImages() == 0);
    REQUIRE(image->GetTexelFormat() == TexelFormat::I_R8_G8_B8);
    REQUIRE(image->GetOriginalTexelFormat() == TexelFormat::I_R8_G8_B8);
    const auto& channels = image->GetImageItem()->colorInfo.sampledTransfer->channels;
    REQUIRE(channels[0] == channels[1]);
    REQUIRE(channels[0] == channels[2]);
    REQUIRE(std::get<std::vector<uint16_t>>(channels[0]->entries).at(128) == 14146);
    REQUIRE(channels[0]->outputScale == 1.0 / 65535.0);
    REQUIRE(image->GetImageItem()->colorInfo.transferFunction == ImageItem::ColorInfo::TransferFunction::Sampled);
    // The first RGB code differs from the red thumbnail. Edge pixels come from the second tile.
    REQUIRE(std::to_integer<uint8_t>(image->GetBufferAt(0, 0)[0]) == 0);
    REQUIRE(std::to_integer<uint8_t>(image->GetBufferAt(16, 8)[0]) == 120);
    REQUIRE(std::to_integer<uint8_t>(image->GetBufferAt(16, 8)[1]) == 64);
    REQUIRE(std::to_integer<uint8_t>(image->GetBufferAt(16, 8)[2]) == 128);
    ImageLoader loader;
    ImageSharedPtr loaded;
    REQUIRE(loader.Decode(bytes.data(), bytes.size(), ImageLoadFlags::None, {}, LLUTILS_TEXT("dng"),
                          PluginTraverseMode::NoTraverse, loaded) == ImageResult::Success);
    REQUIRE(loaded->GetImageItem()->colorInfo.sampledTransfer);
    bytes.clear();
    const auto display = IMUtil::ImageUtil::EncodeLinearImageForDisplay(
        IMUtil::ImageUtil::ApplyTransferFunction(image));
    REQUIRE(std::to_integer<uint8_t>(display->GetBufferAt(16, 8)[0]) == 120);
    REQUIRE(std::to_integer<uint8_t>(display->GetBufferAt(16, 8)[1]) == 64);
    REQUIRE(std::to_integer<uint8_t>(display->GetBufferAt(16, 8)[2]) == 128);
}

TEST_CASE("Unsupported DNG calibration and layout never publish a table-backed image", "[DNG]")
{
    auto bytes         = MakeDng();
    const auto primary = Get(bytes, Entry(bytes, 8, 330) + 8);
    SECTION("Different primaries")
    {
        Put(bytes, Get(bytes, Entry(bytes, 8, 50721) + 8), 10000);
    }
    SECTION("Non-neutral balance")
    {
        Put(bytes, Get(bytes, Entry(bytes, 8, 50727) + 8), 2);
    }
    SECTION("Nonzero exposure")
    {
        Put(bytes, Get(bytes, Entry(bytes, 8, 50730) + 8), 1);
    }
    SECTION("Nonzero black")
    {
        Put(bytes, Get(bytes, Entry(bytes, primary, 50714) + 8), 1);
    }
    SECTION("Different white")
    {
        Put(bytes, Get(bytes, Entry(bytes, primary, 50717) + 8), 255);
    }
    SECTION("Incomplete table")
    {
        Put(bytes, Entry(bytes, primary, 50712) + 4, 255);
    }
    SECTION("Sensor mosaic")
    {
        Put(bytes, Entry(bytes, primary, 262) + 8, 32803, 2);
    }
    IMCodec::ImageSharedPtr image;
    REQUIRE(DecodeDng(bytes, image) == IMCodec::ImageResult::FormatNotSupported);
    REQUIRE_FALSE(image);
}

TEST_CASE("Truncated DNG data and invalid tile offsets fail without publishing pixels", "[DNG]")
{
    auto bytes = MakeDng();
    SECTION("Truncated header")
    {
        bytes.resize(7);
    }
    SECTION("Truncated directory")
    {
        bytes.resize(15);
    }
    SECTION("Out of range tile")
    {
        const auto primary = Get(bytes, Entry(bytes, 8, 330) + 8);
        Put(bytes, Get(bytes, Entry(bytes, primary, 324) + 8), 0xfffffff0);
    }
    SECTION("Invalid JPEG")
    {
        const auto primary    = Get(bytes, Entry(bytes, 8, 330) + 8);
        const auto tileOffset = Get(bytes, Get(bytes, Entry(bytes, primary, 324) + 8));
        bytes[tileOffset]     = std::byte{};
    }
    IMCodec::ImageSharedPtr image;
    REQUIRE(DecodeDng(bytes, image) != IMCodec::ImageResult::Success);
    REQUIRE_FALSE(image);
}

TEST_CASE("Local DNG decodes through the table-preserving TIFF path", "[.][DNGLocal]")
{
    const char* path = std::getenv("OIV_DNG_TEST_FILE");
    if (!path)
        SKIP("Set OIV_DNG_TEST_FILE to exercise a local RGB DNG");
    std::ifstream file(std::filesystem::path(path), std::ios::binary);
    REQUIRE(file.good());
    const std::vector<char> input{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    Bytes bytes(input.size());
    std::memcpy(bytes.data(), input.data(), input.size());
    IMCodec::ImageSharedPtr image;
    REQUIRE(DecodeDng(bytes, image) == IMCodec::ImageResult::Success);
    REQUIRE(image->GetImageItem()->colorInfo.sampledTransfer);
    const auto display = IMUtil::ImageUtil::EncodeLinearImageForDisplay(
        IMUtil::ImageUtil::ApplyTransferFunction(image));
    REQUIRE(display->GetDimensions() == image->GetDimensions());
    bool identical = true;
    for (uint32_t y = 0; y < image->GetHeight(); ++y)
        for (uint32_t x = 0; x < image->GetWidth(); ++x)
            identical &= std::memcmp(display->GetBufferAt(x, y), image->GetBufferAt(x, y), 3) == 0;
    REQUIRE(identical);
}

#if defined(_WIN32)
TEST_CASE("WLX DNG preview applies the table before producing host pixels", "[DNG][wlx][preview]")
{
    auto bytes         = MakeDng();
    const auto primary = Get(bytes, Entry(bytes, 8, 330) + 8);
    const auto table   = Get(bytes, Entry(bytes, primary, 50712) + 8);
    for (size_t i = 0; i < 256; ++i)
        Put(bytes, table + i * 2, 32768, 2);
    struct TemporaryDng
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
                                     ("oiv-transfer-preview-" +
                                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
                                      ".dng");
        ~TemporaryDng()
        {
            std::error_code error;
            std::filesystem::remove(path, error);
        }
    } file;
    {
        std::ofstream stream(file.path, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        REQUIRE(stream.good());
    }
    IMCodec::ImageLoader loader;
    const auto bitmap = OIV::Win32::CreatePreviewBitmap(loader, file.path.c_str(), 1, 1);
    REQUIRE(bitmap != nullptr);
    struct BitmapOwner
    {
        HBITMAP bitmap;
        ~BitmapOwner() { DeleteObject(bitmap); }
    } owner{bitmap};
    DIBSECTION dib{};
    REQUIRE(GetObjectW(bitmap, sizeof(dib), &dib) == sizeof(dib));
    const auto* pixel = static_cast<const uint8_t*>(dib.dsBm.bmBits);
    REQUIRE(pixel[0] == 188);
    REQUIRE(pixel[1] == 188);
    REQUIRE(pixel[2] == 188);
}
#endif

#if defined(OIV_TEST_DNG_FALLBACK)
TEST_CASE("Suitable decoder traversal keeps native DNG priority and reaches the raw fallback", "[DNG][ImageLoader]")
{
    using namespace IMCodec;
    // LibRaw requires at least 22 pixels per axis, independently of native edge-tile coverage.
    auto bytes = MakeDng(33, 25);
    ImageLoader loader;
    ImageSharedPtr image;
    REQUIRE(loader.Decode(bytes.data(), bytes.size(), ImageLoadFlags::None, {}, LLUTILS_TEXT("DnG"),
                          PluginTraverseMode::SuitablePlugin, image) == ImageResult::Success);
    REQUIRE(image->GetImageItem()->colorInfo.sampledTransfer);
    const auto nativeId    = image->GetProcessData().pluginUsed;
    uint32_t expectedWidth = 33, expectedHeight = 25;
    SECTION("Exposure needs raw development")
    {
        Put(bytes, Get(bytes, Entry(bytes, 8, 50730) + 8), 1);
    }
    SECTION("Non-square pixels increase display height")
    {
        const auto primary = Get(bytes, Entry(bytes, 8, 330) + 8);
        Put(bytes, Get(bytes, Entry(bytes, primary, 50718) + 8) + 8, 2);
        expectedHeight = 50;
    }
    SECTION("Non-square pixels increase display width")
    {
        const auto primary = Get(bytes, Entry(bytes, 8, 330) + 8);
        Put(bytes, Get(bytes, Entry(bytes, primary, 50718) + 8), 2);
        expectedWidth = 66;
    }
    image.reset();
    REQUIRE(loader.Decode(bytes.data(), bytes.size(), ImageLoadFlags::None, {}, LLUTILS_TEXT("dng"),
                          PluginTraverseMode::NoTraverse, image) == ImageResult::FormatNotSupported);
    REQUIRE_FALSE(image);
    REQUIRE(loader.Decode(bytes.data(), bytes.size(), ImageLoadFlags::None, {}, LLUTILS_TEXT("dng"),
                          PluginTraverseMode::SuitablePlugin, image) == ImageResult::Success);
    REQUIRE(image);
    REQUIRE(image->GetProcessData().pluginUsed != nativeId);
    REQUIRE_FALSE(image->GetImageItem()->colorInfo.sampledTransfer);
    REQUIRE(image->GetWidth() == expectedWidth);
    REQUIRE(image->GetHeight() == expectedHeight);
}

#endif

TEST_CASE("Suitable traversal does not probe unrelated extensions or hide corrupt DNG tiles", "[DNG][ImageLoader]")
{
    using namespace IMCodec;
    auto bytes = MakeDng();
    ImageLoader loader;
    ImageSharedPtr image;
    SECTION("Unknown extension")
    {
        REQUIRE(loader.Decode(bytes.data(), bytes.size(), ImageLoadFlags::None, {},
                              LLUTILS_TEXT("unregistered-oiv-test"),
                              PluginTraverseMode::SuitablePlugin | PluginTraverseMode::AnyFileType,
                              image) != ImageResult::Success);
    }
    SECTION("Unrelated extension")
    {
        REQUIRE(loader.Decode(bytes.data(), bytes.size(), ImageLoadFlags::None, {}, LLUTILS_TEXT("jpg"),
                              PluginTraverseMode::SuitablePlugin | PluginTraverseMode::AnyFileType,
                              image) != ImageResult::Success);
    }
    SECTION("Corrupt tile")
    {
        const auto primary    = Get(bytes, Entry(bytes, 8, 330) + 8);
        const auto tileOffset = Get(bytes, Get(bytes, Entry(bytes, primary, 324) + 8));
        bytes[tileOffset]     = std::byte{};
        REQUIRE(loader.Decode(bytes.data(), bytes.size(), ImageLoadFlags::None, {}, LLUTILS_TEXT("dng"),
                              PluginTraverseMode::SuitablePlugin, image) == ImageResult::FileIsCorrupted);
    }
    REQUIRE_FALSE(image);
}

TEST_CASE("Native DNG accepts square pixels and rejects non-square or invalid default scale", "[DNG]")
{
    auto bytes         = MakeDng();
    const auto primary = Get(bytes, Entry(bytes, 8, 330) + 8);
    const auto scale   = Get(bytes, Entry(bytes, primary, 50718) + 8);
    auto expected      = IMCodec::ImageResult::Success;
    SECTION("Unit scale") {}
    SECTION("Uniform scale preserves pixel aspect")
    {
        Put(bytes, scale, 2);
        Put(bytes, scale + 8, 2);
    }
    SECTION("Non-square scale needs fallback")
    {
        Put(bytes, scale + 8, 2);
        expected = IMCodec::ImageResult::FormatNotSupported;
    }
    SECTION("Zero scale is invalid")
    {
        Put(bytes, scale, 0);
        Put(bytes, scale + 8, 0);
        expected = IMCodec::ImageResult::FormatNotSupported;
    }
    IMCodec::ImageSharedPtr image;
    REQUIRE(DecodeDng(bytes, image) == expected);
    if (expected == IMCodec::ImageResult::Success)
    {
        REQUIRE(image);
        REQUIRE(image->GetWidth() == 17);
        REQUIRE(image->GetHeight() == 9);
    }
    else
        REQUIRE_FALSE(image);
}
