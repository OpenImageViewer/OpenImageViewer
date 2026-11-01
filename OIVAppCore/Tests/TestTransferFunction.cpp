#include <ImageUtil/ImageUtil.h>
#include <ImageLoader.h>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <iterator>
#include <OIVAppCore/ImageState.h>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <numeric>
#include <limits>

namespace
{
    using namespace IMCodec;
    using Info = ImageItem::ColorInfo;

    SampledTransferFunction SharedTransfer(
        SampledTransferCurve::Entries entries,
        SampledTransferCurve::Indexing indexing = SampledTransferCurve::Indexing::DirectInteger,
        double scale                            = 1.0 / 65535.0)
    {
        const auto curve = std::make_shared<const SampledTransferCurve>(
            SampledTransferCurve{.entries = std::move(entries), .indexing = indexing, .outputScale = scale});
        return {.channels = {curve, curve, curve}};
    }

    ImageSharedPtr MakeTableImage(bool alpha = false)
    {
        auto item        = std::make_shared<ImageItem>();
        item->descriptor = {.width                   = 2,
                            .height                  = 2,
                            .rowPitchInBytes         = 11,
                            .texelFormatDecompressed = alpha ? TexelFormat::I_R8_G8_B8_A8 : TexelFormat::I_R8_G8_B8};
        item->data.Allocate(22);
        std::fill_n(item->data.data(), 22, std::byte{0xcc});
        const std::array<uint8_t, 8> pixels{128, 64, 255, 17, 0, 10, 192, 255};
        for (size_t y = 0; y < 2; ++y)
        {
            auto* row = item->data.data() + y * 11;
            if (alpha)
                std::memcpy(row, pixels.data(), pixels.size());
            else
            {
                std::memcpy(row, pixels.data(), 3);
                std::memcpy(row + 3, pixels.data() + 4, 3);
            }
        }
        std::vector<uint16_t> table(256);
        for (size_t i = 0; i < table.size(); ++i)
        {
            const double encoded = static_cast<double>(i) / 255.0;
            const double linear  = encoded <= 0.04045 ? encoded / 12.92 : std::pow((encoded + 0.055) / 1.055, 2.4);
            table[i]             = static_cast<uint16_t>(std::lround(linear * 65535.0));
        }
        item->colorInfo.encoding         = Info::Encoding::SRGB;
        item->colorInfo.transferFunction = Info::TransferFunction::Sampled;
        item->colorInfo.alphaMode        = alpha ? Info::AlphaMode::Straight : Info::AlphaMode::Opaque;
        item->colorInfo.sampledTransfer  = SharedTransfer(std::move(table));
        item->metaData                   = std::make_shared<ItemMetaData>();
        return std::make_shared<Image>(item, ImageItemType::Unknown);
    }

    template <class T>
    std::array<T, 4> ReadPixel(const ImageSharedPtr& image, int x, int y)
    {
        std::array<T, 4> result;
        std::memcpy(result.data(), image->GetBufferAt(x, y), sizeof(result));
        return result;
    }
}  // namespace

TEST_CASE("Table transfer preserves original codes and exact 16-bit linear values", "[TransferFunction]")
{
    for (bool alpha : {false, true})
    {
        auto source = MakeTableImage(alpha);
        const std::vector<std::byte> original(source->GetBuffer(), source->GetBuffer() + 22);
        const auto linear = IMUtil::ImageUtil::ApplyTransferFunction(source);
        REQUIRE(linear != source);
        REQUIRE(linear->GetTexelFormat() == TexelFormat::I_R16_G16_B16_A16);
        REQUIRE(linear->GetOriginalTexelFormat() == source->GetOriginalTexelFormat());
        REQUIRE(ReadPixel<uint16_t>(linear, 0, 0) ==
                std::array<uint16_t, 4>{14146, 3360, 65535, static_cast<uint16_t>(alpha ? 17 * 257 : 65535)});
        REQUIRE(ReadPixel<uint16_t>(linear, 1, 1) == std::array<uint16_t, 4>{0, 199, 34544, 65535});
        REQUIRE(std::equal(original.begin(), original.end(), source->GetBuffer()));
        REQUIRE(linear->GetImageItem()->metaData == source->GetImageItem()->metaData);
        REQUIRE(linear->GetImageItem()->colorInfo.transferFunction == Info::TransferFunction::Linear);
        REQUIRE_FALSE(linear->GetImageItem()->colorInfo.sampledTransfer);
        REQUIRE(source->GetImageItem()->colorInfo.sampledTransfer);
        REQUIRE(IMUtil::ImageUtil::ApplyTransferFunction(linear) == linear);
        const auto display = IMUtil::ImageUtil::EncodeLinearImageForDisplay(linear);
        REQUIRE(display->GetOriginalTexelFormat() == source->GetOriginalTexelFormat());
        REQUIRE(ReadPixel<uint8_t>(display, 0, 0) ==
                std::array<uint8_t, 4>{128, 64, 255, static_cast<uint8_t>(alpha ? 17 : 255)});
        REQUIRE(ReadPixel<uint8_t>(display, 1, 1) == std::array<uint8_t, 4>{0, 10, 192, 255});
        REQUIRE(display->GetImageItem()->colorInfo.transferFunction == Info::TransferFunction::SRGB);
        REQUIRE_FALSE(display->GetImageItem()->colorInfo.sampledTransfer);
        REQUIRE(IMUtil::ImageUtil::EncodeLinearImageForDisplay(display) == display);
    }
}

TEST_CASE("Display transfer uses fixed sRGB encoding for an arbitrary linearization table", "[TransferFunction]")
{
    auto source = MakeTableImage();
    std::vector<uint16_t> table(256, 32768);
    source->GetImageItem()->colorInfo.sampledTransfer = SharedTransfer(std::move(table));
    const auto linear                                 = IMUtil::ImageUtil::ApplyTransferFunction(source);
    REQUIRE(ReadPixel<uint16_t>(linear, 0, 0) == std::array<uint16_t, 4>{32768, 32768, 32768, 65535});
    const auto display = IMUtil::ImageUtil::EncodeLinearImageForDisplay(linear);
    REQUIRE(ReadPixel<uint8_t>(display, 0, 0) == std::array<uint8_t, 4>{188, 188, 188, 255});
    source->GetImageItem()->colorInfo.transferFunction = Info::TransferFunction::Unknown;
    REQUIRE(IMUtil::ImageUtil::ApplyTransferFunction(source) == source);
    REQUIRE(IMUtil::ImageUtil::EncodeLinearImageForDisplay(source) == source);
}

TEST_CASE("Transfer rejects malformed metadata and image layouts", "[TransferFunction]")
{
    auto image = MakeTableImage();
    SECTION("Missing table")
    {
        image->GetImageItem()->colorInfo.sampledTransfer.reset();
    }
    SECTION("Incomplete table")
    {
        image->GetImageItem()->colorInfo.sampledTransfer = SharedTransfer(std::vector<uint16_t>(255));
    }
    SECTION("Unsupported primaries")
    {
        image->GetImageItem()->colorInfo.encoding = Info::Encoding::Unknown;
    }
    SECTION("ICC ambiguity")
    {
        image->GetImageItem()->colorInfo.iccProfile = std::make_shared<const std::vector<std::byte>>(4);
    }
    SECTION("Short row")
    {
        image->GetImageItem()->descriptor.rowPitchInBytes = 5;
    }
    SECTION("Short buffer")
    {
        image->GetImageItem()->data.Allocate(6);
    }
    SECTION("Unrepresentable output width")
    {
        image->GetImageItem()->descriptor.width = std::numeric_limits<uint32_t>::max();
    }
    SECTION("Unrepresentable output height")
    {
        image->GetImageItem()->descriptor.height = std::numeric_limits<uint32_t>::max();
    }
    SECTION("Wrong format")
    {
        image->GetImageItem()->descriptor.texelFormatDecompressed = TexelFormat::I_X8;
    }
    REQUIRE_THROWS(IMUtil::ImageUtil::ApplyTransferFunction(image));
}

TEST_CASE("ImageState caches transfer after geometry and retains original inspection pixels",
          "[TransferFunction][ImageState]")
{
    OIV::ImageState state;
    auto source = MakeTableImage();
    state.SetSelectedSource(source);
    state.Refresh(OIV::ImageProcessingStage::GeometricTransform);
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::GeometricTransform) == source);
    REQUIRE_FALSE(state.GetRetainedImage(OIV::ImageProcessingStage::TransferFunction));
    state.Refresh();
    const auto linear  = state.GetRetainedImage(OIV::ImageProcessingStage::TransferFunction);
    const auto display = state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat);
    REQUIRE(linear->GetTexelFormat() == TexelFormat::I_R16_G16_B16_A16);
    REQUIRE(display->GetTexelFormat() == TexelFormat::I_R8_G8_B8_A8);
    REQUIRE(state.GetSelectedSource() == source);
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::GeometricTransform) == source);
    state.SetScale({0.5, 0.5});
    state.SetOffset({3, 4});
    state.SetResample(true);
    state.Refresh();
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::TransferFunction) == linear);
    state.SetUseRainbowNormalization(true);
    state.Refresh();
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::TransferFunction) == linear);
    REQUIRE(ReadPixel<uint8_t>(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat), 0, 0) ==
            ReadPixel<uint8_t>(display, 0, 0));
    state.Transform(IMUtil::AxisAlignedRotation::Rotate180, IMUtil::AxisAlignedFlip::None);
    REQUIRE_FALSE(state.GetRetainedImage(OIV::ImageProcessingStage::TransferFunction));
    state.Refresh();
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::TransferFunction) != linear);
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::GeometricTransform)
                ->GetImageItem()
                ->colorInfo.sampledTransfer->channels == source->GetImageItem()->colorInfo.sampledTransfer->channels);
    state.SetSelectedSource(MakeTableImage());
    REQUIRE_FALSE(state.GetRetainedImage(OIV::ImageProcessingStage::TransferFunction));
    state.ClearAll();
    state.Refresh();
    REQUIRE_FALSE(state.GetVisibleImage());
}

TEST_CASE("ImageState shares the transfer stage for ordinary images", "[TransferFunction][ImageState]")
{
    auto image                                        = MakeTableImage(true);
    image->GetImageItem()->colorInfo.transferFunction = Info::TransferFunction::Unknown;
    image->GetImageItem()->colorInfo.sampledTransfer.reset();
    OIV::ImageState state;
    state.SetSelectedSource(image);
    state.Refresh();
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::TransferFunction) == image);
    REQUIRE(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == image);
}

TEST_CASE("Table metadata survives pitch packing and cropping without applying it", "[TransferFunction]")
{
    const auto source  = MakeTableImage(true);
    const auto packed  = IMUtil::ImageUtil::NormalizePitch(source);
    const auto cropped = IMUtil::ImageUtil::CropImage(packed, {{0, 0}, {1, 1}});
    for (const auto& image : {packed, cropped})
    {
        REQUIRE(image->GetImageItem()->colorInfo.sampledTransfer->channels ==
                source->GetImageItem()->colorInfo.sampledTransfer->channels);
        REQUIRE(image->GetImageItem()->colorInfo.transferFunction == Info::TransferFunction::Sampled);
        REQUIRE(ReadPixel<uint8_t>(image, 0, 0) == ReadPixel<uint8_t>(source, 0, 0));
        REQUIRE(ReadPixel<uint16_t>(IMUtil::ImageUtil::ApplyTransferFunction(image), 0, 0)[0] == 14146);
    }
}

TEST_CASE("Sampled transfer accepts shared 8-bit entries and explicit output scaling", "[TransferFunction]")
{
    auto source = MakeTableImage();
    std::vector<uint8_t> entries(256);
    std::iota(entries.begin(), entries.end(), uint8_t{0});
    source->GetImageItem()->colorInfo.sampledTransfer = SharedTransfer(std::move(entries),
                                                                       SampledTransferCurve::Indexing::DirectInteger,
                                                                       1.0 / 255.0);
    REQUIRE(ReadPixel<uint16_t>(IMUtil::ImageUtil::ApplyTransferFunction(source), 0, 0) ==
            std::array<uint16_t, 4>{32896, 16448, 65535, 65535});
    source->GetImageItem()->colorInfo.sampledTransfer = SharedTransfer(std::vector<uint8_t>(256, 1),
                                                                       SampledTransferCurve::Indexing::DirectInteger,
                                                                       0.5);
    REQUIRE(ReadPixel<uint16_t>(IMUtil::ImageUtil::ApplyTransferFunction(source), 0, 0) ==
            std::array<uint16_t, 4>{32768, 32768, 32768, 65535});
    source->GetImageItem()->colorInfo.sampledTransfer = SharedTransfer(std::vector<uint16_t>(256, 1000),
                                                                       SampledTransferCurve::Indexing::DirectInteger,
                                                                       1.0 / 2000.0);
    REQUIRE(ReadPixel<uint16_t>(IMUtil::ImageUtil::ApplyTransferFunction(source), 0, 0) ==
            std::array<uint16_t, 4>{32768, 32768, 32768, 65535});
}

TEST_CASE("Independent sampled RGB curves interpolate mixed entry types and preserve alpha", "[TransferFunction]")
{
    auto source    = MakeTableImage(true);
    const auto red = SharedTransfer(std::vector<uint8_t>{0, 255}, SampledTransferCurve::Indexing::NormalizedLinear,
                                    1.0 / 255.0)
                         .channels[0];
    const auto green =
        SharedTransfer(std::vector<uint16_t>{0, 32768}, SampledTransferCurve::Indexing::NormalizedLinear).channels[0];
    const auto blue =
        SharedTransfer(std::vector<uint16_t>{12345}, SampledTransferCurve::Indexing::NormalizedLinear).channels[0];
    source->GetImageItem()->colorInfo.sampledTransfer = SampledTransferFunction{.channels = {red, green, blue}};
    const auto linear                                 = IMUtil::ImageUtil::ApplyTransferFunction(source);
    REQUIRE(ReadPixel<uint16_t>(linear, 0, 0) == std::array<uint16_t, 4>{32896, 8224, 12345, 4369});
    REQUIRE(ReadPixel<uint16_t>(linear, 1, 1)[0] == 0);
}

TEST_CASE("Sampled curves support unsigned 16-bit samples across unaligned padded rows", "[TransferFunction]")
{
    for (bool alpha : {false, true})
    {
        auto item            = std::make_shared<ImageItem>();
        const uint32_t pitch = alpha ? 19 : 15;
        item->descriptor     = {.width                   = 2,
                                .height                  = 2,
                                .rowPitchInBytes         = pitch,
                                .texelFormatDecompressed = alpha ? TexelFormat::I_R16_G16_B16_A16
                                                                 : TexelFormat::I_R16_G16_B16};
        item->data.Allocate(pitch * 2);
        std::fill_n(item->data.data(), pitch * 2, std::byte{0xcc});
        const std::array<uint16_t, 8> samples{0, 32768, 65535, 12345, 65535, 0, 32768, 65535};
        for (size_t y = 0; y < 2; ++y)
            for (size_t x = 0; x < 2; ++x)
                std::memcpy(item->data.data() + y * pitch + x * (alpha ? 8 : 6), samples.data() + x * 4, alpha ? 8 : 6);
        item->colorInfo.encoding         = Info::Encoding::SRGB;
        item->colorInfo.transferFunction = Info::TransferFunction::Sampled;
        item->colorInfo.alphaMode        = alpha ? Info::AlphaMode::Straight : Info::AlphaMode::Opaque;
        auto source                      = std::make_shared<Image>(item, ImageItemType::Unknown);
        const std::vector<std::byte> original(source->GetBuffer(), source->GetBuffer() + pitch * 2);
        item->colorInfo.sampledTransfer = SharedTransfer(std::vector<uint8_t>{0, 255},
                                                         SampledTransferCurve::Indexing::NormalizedLinear, 1.0 / 255.0);
        auto linear                     = IMUtil::ImageUtil::ApplyTransferFunction(source);
        REQUIRE(ReadPixel<uint16_t>(linear, 0, 1) ==
                std::array<uint16_t, 4>{0, 32768, 65535, static_cast<uint16_t>(alpha ? 12345 : 65535)});
        REQUIRE(ReadPixel<uint16_t>(linear, 1, 1) == std::array<uint16_t, 4>{65535, 0, 32768, 65535});
        std::vector<uint16_t> direct(65536);
        std::iota(direct.begin(), direct.end(), uint16_t{0});
        item->colorInfo.sampledTransfer = SharedTransfer(std::move(direct));
        linear                          = IMUtil::ImageUtil::ApplyTransferFunction(source);
        REQUIRE(ReadPixel<uint16_t>(linear, 0, 1) ==
                std::array<uint16_t, 4>{0, 32768, 65535, static_cast<uint16_t>(alpha ? 12345 : 65535)});
        REQUIRE(std::equal(original.begin(), original.end(), source->GetBuffer()));
        REQUIRE_FALSE(linear->GetImageItem()->colorInfo.sampledTransfer);
    }
}

TEST_CASE("Malformed sampled curves are rejected before pixel conversion", "[TransferFunction]")
{
    auto source = MakeTableImage();
    SampledTransferCurve curve{.entries     = std::vector<uint8_t>{0, 255},
                               .indexing    = SampledTransferCurve::Indexing::NormalizedLinear,
                               .outputScale = 1.0 / 255.0};
    SECTION("Empty samples")
    {
        curve.entries = std::vector<uint16_t>{};
    }
    SECTION("Incomplete direct domain")
    {
        curve.indexing = SampledTransferCurve::Indexing::DirectInteger;
    }
    SECTION("Unknown indexing")
    {
        curve.indexing = static_cast<SampledTransferCurve::Indexing>(99);
    }
    SECTION("Zero scale")
    {
        curve.outputScale = 0;
    }
    SECTION("Negative scale")
    {
        curve.outputScale = -1;
    }
    SECTION("NaN scale")
    {
        curve.outputScale = std::numeric_limits<double>::quiet_NaN();
    }
    SECTION("Infinite scale")
    {
        curve.outputScale = std::numeric_limits<double>::infinity();
    }
    SECTION("HDR output")
    {
        curve.outputScale = 1;
    }
    auto transfer        = SharedTransfer(std::vector<uint16_t>(256));
    transfer.channels[1] = std::make_shared<const SampledTransferCurve>(std::move(curve));
    SECTION("Missing green curve")
    {
        transfer.channels[1].reset();
    }
    source->GetImageItem()->colorInfo.sampledTransfer = std::move(transfer);
    REQUIRE_THROWS(IMUtil::ImageUtil::ApplyTransferFunction(source));
}

TEST_CASE("Derived image operations preserve metadata and own independent packed pixels",
          "[TransferFunction][ImageUtil]")
{
    const auto source                       = MakeTableImage(true);
    source->GetImageItem()->colorInfo.gamma = 0.5;
    const std::vector<std::byte> original(source->GetBuffer(), source->GetBuffer() + source->GetSizeInMemory());
    const auto converted = IMUtil::ImageUtil::Convert(source, TexelFormat::I_B8_G8_R8_A8);
    const auto cropped   = IMUtil::ImageUtil::GetSubImage(source, {{1, 0}, {2, 2}});
    const auto packed    = IMUtil::ImageUtil::NormalizePitch(source);
    REQUIRE(ReadPixel<uint8_t>(converted, 0, 0) == std::array<uint8_t, 4>{255, 64, 128, 17});
    REQUIRE(cropped->GetWidth() == 1);
    REQUIRE(cropped->GetHeight() == 2);
    REQUIRE(ReadPixel<uint8_t>(cropped, 0, 1) == std::array<uint8_t, 4>{0, 10, 192, 255});
    for (const auto& image : {converted, cropped, packed})
    {
        REQUIRE(image->GetBuffer() != source->GetBuffer());
        REQUIRE(image->GetIsRowPitchNormalized());
        REQUIRE(image->GetImageItem()->data.Size() == image->GetTotalSizeOfImageTexels());
        REQUIRE(image->GetOriginalTexelFormat() == source->GetOriginalTexelFormat());
        REQUIRE(image->GetImageItem()->metaData == source->GetImageItem()->metaData);
        REQUIRE(image->GetImageItem()->colorInfo.sampledTransfer->channels ==
                source->GetImageItem()->colorInfo.sampledTransfer->channels);
        REQUIRE(image->GetImageItem()->colorInfo.gamma == 0.5);
        REQUIRE(image->GetImageItem()->colorInfo.transferFunction == Info::TransferFunction::Sampled);
    }
    converted->GetImageItem()->data.data()[0] = std::byte{};
    REQUIRE(std::equal(original.begin(), original.end(), source->GetBuffer()));
    REQUIRE(IMUtil::ImageUtil::Convert(source, source->GetTexelFormat()) == source);
    REQUIRE_FALSE(IMUtil::ImageUtil::GetSubImage(source, {{-1, 0}, {1, 1}}));
}

TEST_CASE("Direct curves must be applied before RGB channel bit depths change", "[TransferFunction][ImageUtil]")
{
    auto item        = std::make_shared<ImageItem>();
    item->descriptor = {.width                   = 1,
                        .height                  = 1,
                        .rowPitchInBytes         = 8,
                        .texelFormatDecompressed = TexelFormat::I_R16_G16_B16_A16};
    item->data.Allocate(8);
    const std::array<uint16_t, 4> samples{32768, 16384, 65535, 65535};
    std::memcpy(item->data.data(), samples.data(), sizeof(samples));
    std::vector<uint16_t> entries(65536);
    std::iota(entries.begin(), entries.end(), uint16_t{});
    item->colorInfo.encoding         = Info::Encoding::SRGB;
    item->colorInfo.transferFunction = Info::TransferFunction::Sampled;
    item->colorInfo.alphaMode        = Info::AlphaMode::Straight;
    item->colorInfo.sampledTransfer  = SharedTransfer(std::move(entries));
    const auto source                = std::make_shared<Image>(item, ImageItemType::Unknown);
    REQUIRE_FALSE(IMUtil::ImageUtil::Convert(source, TexelFormat::I_R8_G8_B8_A8));
    REQUIRE_FALSE(IMUtil::ImageUtil::Convert(source, TexelFormat::I_B8_G8_R8_A8));
    REQUIRE(ReadPixel<uint16_t>(source, 0, 0) == samples);
    REQUIRE(source->GetImageItem()->colorInfo.sampledTransfer);
    const auto display = IMUtil::ImageUtil::EncodeLinearImageForDisplay(
        IMUtil::ImageUtil::ApplyTransferFunction(source));
    REQUIRE(ReadPixel<uint8_t>(display, 0, 0) == std::array<uint8_t, 4>{188, 137, 255, 255});
    REQUIRE(IMUtil::ImageUtil::Convert(display, TexelFormat::I_B8_G8_R8_A8));
    item->colorInfo.sampledTransfer = SharedTransfer(std::vector<uint8_t>{0, 255},
                                                     SampledTransferCurve::Indexing::NormalizedLinear, 1.0 / 255.0);
    const auto normalized           = IMUtil::ImageUtil::Convert(source, TexelFormat::I_R8_G8_B8_A8);
    REQUIRE(normalized);
    REQUIRE(normalized->GetImageItem()->colorInfo.sampledTransfer->channels ==
            item->colorInfo.sampledTransfer->channels);
    REQUIRE(ReadPixel<uint16_t>(IMUtil::ImageUtil::ApplyTransferFunction(normalized), 0, 0) ==
            std::array<uint16_t, 4>{32896, 16448, 65535, 65535});
}

TEST_CASE("Direct curves allow RGB repacking and alpha removal at the same channel depth",
          "[TransferFunction][ImageUtil]")
{
    const auto source = MakeTableImage(true);
    const auto rgb    = IMUtil::ImageUtil::Convert(source, TexelFormat::I_R8_G8_B8);
    REQUIRE(rgb);
    REQUIRE(rgb->GetImageItem()->colorInfo.sampledTransfer->channels ==
            source->GetImageItem()->colorInfo.sampledTransfer->channels);
    const auto rgba = IMUtil::ImageUtil::Convert(rgb, TexelFormat::I_R8_G8_B8_A8);
    REQUIRE(rgba);
    REQUIRE(ReadPixel<uint8_t>(rgba, 0, 0) == std::array<uint8_t, 4>{128, 64, 255, 255});
    REQUIRE(IMUtil::ImageUtil::Convert(source, TexelFormat::I_B8_G8_R8_A8));
}

#if defined(OIV_TEST_TRANSFER_ENCODERS)
TEST_CASE("PNG and JPEG require sampled transfers to be baked before encoding", "[TransferFunction][ImageEncoding]")
{
    auto source                                       = MakeTableImage(true);
    source->GetImageItem()->colorInfo.sampledTransfer = SharedTransfer(std::vector<uint16_t>(256, 32768));
    const std::vector<std::byte> original(source->GetBuffer(), source->GetBuffer() + source->GetSizeInMemory());
    const auto linear8 = IMUtil::ImageUtil::Convert(IMUtil::ImageUtil::ApplyTransferFunction(source),
                                                    TexelFormat::I_R8_G8_B8_A8);
    REQUIRE(linear8);
    REQUIRE(linear8->GetImageItem()->colorInfo.transferFunction == Info::TransferFunction::Linear);
    REQUIRE(ReadPixel<uint8_t>(linear8, 0, 0) == std::array<uint8_t, 4>{128, 128, 128, 17});
    ImageLoader loader;
    for (const auto& unencoded : {source, linear8})
        for (const auto* extension : {LLUTILS_TEXT("png"), LLUTILS_TEXT("jpg")})
        {
            LLUtils::Buffer rejected(4);
            std::fill_n(rejected.data(), rejected.size(), std::byte{42});
            REQUIRE(loader.Encode(unencoded, extension, rejected) == ImageResult::FormatNotSupported);
            REQUIRE(rejected.size() == 4);
            REQUIRE(std::ranges::all_of(std::span(rejected.data(), rejected.size()),
                                        [](std::byte value) { return value == std::byte{42}; }));
        }
    OIV::ImageState state;
    state.SetSelectedSource(source);
    state.Refresh(OIV::ImageProcessingStage::RenderFormat);
    const auto ready = state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat);
    REQUIRE_FALSE(ready->GetImageItem()->colorInfo.sampledTransfer);
    REQUIRE(ReadPixel<uint8_t>(ready, 0, 0) == std::array<uint8_t, 4>{188, 188, 188, 17});
    for (const auto* extension : {LLUTILS_TEXT("png"), LLUTILS_TEXT("jpg")})
    {
        LLUtils::Buffer encoded;
        REQUIRE(loader.Encode(ready, extension, encoded) == ImageResult::Success);
        REQUIRE(encoded.size() > 0);
        ImageSharedPtr decoded;
        REQUIRE(loader.Decode(encoded.data(), encoded.size(), ImageLoadFlags::None, {}, extension,
                              PluginTraverseMode::NoTraverse, decoded) == ImageResult::Success);
        REQUIRE_FALSE(decoded->GetImageItem()->colorInfo.sampledTransfer);
        if (LLUtils::native_string_view(extension) == LLUTILS_TEXT("png"))
            REQUIRE(ReadPixel<uint8_t>(decoded, 0, 0) == std::array<uint8_t, 4>{188, 188, 188, 17});
    }
    REQUIRE(std::equal(original.begin(), original.end(), source->GetBuffer()));
    REQUIRE(source->GetImageItem()->colorInfo.transferFunction == Info::TransferFunction::Sampled);
}

TEST_CASE("Failed sampled-transfer encoding does not create or truncate destination files",
          "[TransferFunction][ImageEncoding]")
{
    struct Files
    {
        const std::filesystem::path existing =
            std::filesystem::temp_directory_path() /
            ("oiv-transfer-save-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) +
             ".png");
        const std::filesystem::path missing = existing.parent_path() / (existing.stem().string() + "-new.png");
        ~Files()
        {
            std::error_code error;
            std::filesystem::remove(existing, error);
            std::filesystem::remove(missing, error);
        }
    } files;
    {
        std::ofstream stream(files.existing, std::ios::binary);
        stream << "keep existing file";
        REQUIRE(stream.good());
    }
    auto source = MakeTableImage(true);
    ImageLoader loader;
    const auto linear8 = IMUtil::ImageUtil::Convert(IMUtil::ImageUtil::ApplyTransferFunction(source),
                                                    TexelFormat::I_R8_G8_B8_A8);
    for (const auto& unencoded : {source, linear8})
    {
        REQUIRE(loader.Encode(unencoded, files.existing.native()) == ImageResult::FormatNotSupported);
        REQUIRE(loader.Encode(unencoded, files.missing.native()) == ImageResult::FormatNotSupported);
    }
    REQUIRE_FALSE(std::filesystem::exists(files.missing));
    {
        std::ifstream stream(files.existing, std::ios::binary);
        const std::string contents{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        REQUIRE(contents == "keep existing file");
    }
    OIV::ImageState state;
    state.SetSelectedSource(source);
    state.Refresh(OIV::ImageProcessingStage::RenderFormat);
    REQUIRE(loader.Encode(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat), files.missing.native()) ==
            ImageResult::Success);
    REQUIRE(std::filesystem::file_size(files.missing) > 0);
}
#endif
