#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <OIVAppCore/ImageState.h>
#include <ImageProcessing/Resampler.h>

#include <algorithm>
#include <cstring>
#include <bit>
#include <random>

namespace
{
    IMCodec::ImageSharedPtr CreateImage(uint32_t width, uint32_t height)
    {
        auto item                                = std::make_shared<IMCodec::ImageItem>();
        item->descriptor.width                   = width;
        item->descriptor.height                  = height;
        item->descriptor.rowPitchInBytes         = width * 4;
        item->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
        item->data.Allocate(static_cast<size_t>(width) * height * 4);
        std::fill_n(item->data.data(), item->data.Size(), std::byte{0x80});
        return std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown);
    }

    uint32_t ReadPixel(const IMCodec::ImageSharedPtr& image, int x, int y)
    {
        uint32_t pixel;
        std::memcpy(&pixel, image->GetBufferAt(x, y), sizeof(pixel));
        return pixel;
    }

    void WritePixel(const IMCodec::ImageSharedPtr& image, int x, int y, uint32_t pixel)
    {
        auto& item = image->GetImageItem();
        std::memcpy(item->data.data() + static_cast<size_t>(y) * item->descriptor.rowPitchInBytes + x * sizeof(pixel),
                    &pixel, sizeof(pixel));
    }

    void CheckExtent(OIV::ImageState& state, double width, double height)
    {
        const auto extent = state.GetVisibleSize();
        CHECK(extent.x == Catch::Approx(width));
        CHECK(extent.y == Catch::Approx(height));
    }
}  // namespace

TEST_CASE("ImageState reuses images within the display limit", "[AppCore][ImageState]")
{
    const auto size = GENERATE(LLUtils::PointI32{17, 9}, LLUtils::PointI32{16384, 1}, LLUtils::PointI32{1, 16384});
    auto original   = CreateImage(size.x, size.y);
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.SetScale({0.5, 0.25});
    state.Refresh();

    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == original);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled) == original);
    CHECK(state.GetVisibleImage() == original);
    CHECK_FALSE(state.IsDownscaled());
    CheckExtent(state, size.x * 0.5, size.y * 0.25);
}

TEST_CASE("ImageState caps thin and odd-sized images without changing original coordinates", "[AppCore][ImageState]")
{
    const auto size = GENERATE(LLUtils::PointI32{16385, 1}, LLUtils::PointI32{1, 16385}, LLUtils::PointI32{32769, 5},
                               LLUtils::PointI32{5, 32769});
    auto original   = CreateImage(size.x, size.y);
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.Refresh();
    auto display          = state.GetVisibleImage();
    const auto dimensions = display->GetDimensions();

    REQUIRE(state.IsDownscaled());
    CHECK(display != original);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == original);
    CHECK(original->GetWidth() == size.x);
    CHECK(original->GetHeight() == size.y);
    CHECK(state.GetVisibleImage() != original);
    CHECK(state.GetVisibleImage() == display);
    CHECK(std::max(dimensions.x, dimensions.y) == 16384);
    CHECK(std::min(dimensions.x, dimensions.y) == (std::min(size.x, size.y) == 1 ? 1 : 2));
    CHECK(ReadPixel(display, 0, 0) == 0x80808080);
    CHECK(ReadPixel(display, dimensions.x - 1, dimensions.y - 1) == 0x80808080);

    for (const double zoom : {0.25, 1.0, 2.0})
    {
        state.SetScale({zoom, zoom});
        state.SetOffset({19.0, -7.0});
        state.Refresh();
        CHECK(state.GetVisibleImage() == display);
        CHECK(state.GetPresentation().offset == LLUtils::PointF64{19.0, -7.0});
        CheckExtent(state, size.x * zoom, size.y * zoom);
    }
}

TEST_CASE("ImageState retains the capped copy across software resampling and invalidates changed content",
          "[AppCore][ImageState]")
{
    auto original = CreateImage(32769, 9);
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.Refresh();
    auto capped = state.GetVisibleImage();

    state.SetScale({0.1, 0.1});
    state.SetResample(true);
    state.Refresh();
    auto resampled = state.GetVisibleImage();
    REQUIRE(resampled != capped);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled) == capped);
    CHECK(state.GetVisibleImage() != capped);
    CHECK(state.GetVisibleImage() == resampled);
    CHECK(resampled->GetWidth() == 3277);
    CHECK(resampled->GetHeight() == 1);
    CheckExtent(state, 3276.9, 0.9);

    state.SetResample(false);
    CHECK(state.GetVisibleImage() == capped);
    CHECK(state.GetVisibleImage() != resampled);
    CheckExtent(state, 3276.9, 0.9);

    state.SetResample(true);
    state.SetScale({2.0, 2.0});
    state.Refresh();
    CHECK(state.GetVisibleImage() == capped);
    CheckExtent(state, 65538.0, 18.0);

    state.SetUseRainbowNormalization(true);
    state.Refresh();
    auto normalized = state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled);
    CHECK(normalized != capped);
    CHECK(state.GetVisibleImage() != capped);

    state.Transform(IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::None);
    state.Refresh();
    auto rotated = state.GetVisibleImage();
    CHECK(rotated->GetWidth() == 4);
    CHECK(rotated->GetHeight() == 16384);
    CHECK(state.GetVisibleImage() != normalized);
    CheckExtent(state, 18.0, 65538.0);

    // Frame changes and edits both replace the chain root.
    auto replacement = CreateImage(12, 8);
    state.SetSelectedSource(replacement);
    state.ResetUserState();
    state.Refresh();
    CHECK_FALSE(state.IsDownscaled());
    CHECK(state.GetVisibleImage() != rotated);
    CHECK(state.GetVisibleImage() == replacement);
    state.ClearAll();
    CHECK(state.GetVisibleImage() == nullptr);
    CHECK_FALSE(state.IsDownscaled());
}

TEST_CASE("Resampler covers edges of thin images and near-unity reductions", "[AppCore][ImageState][Resampler]")
{
    const auto size                = GENERATE(LLUtils::PointI32{5, 1}, LLUtils::PointI32{1, 5}, LLUtils::PointI32{5, 3},
                                              LLUtils::PointI32{3, 5});
    auto original                  = CreateImage(size.x, size.y);
    const bool horizontal          = size.x > size.y;
    const LLUtils::PointI32 target = horizontal ? LLUtils::PointI32{4, size.y} : LLUtils::PointI32{size.x, 4};
    auto sampled                   = OIV::Resampler{}.Resample(original, target);
    for (int y = 0; y < target.y; ++y)
        for (int x = 0; x < target.x; ++x)
            CHECK(ReadPixel(sampled, x, y) == 0x80808080);

    // Each sample averages a pair of source pixels along just one axis.
    auto strip = CreateImage(horizontal ? 4 : 1, horizontal ? 1 : 4);
    const uint32_t pixels[]{0x00000000, 0x20202020, 0x40404040, 0x60606060};
    std::memcpy(strip->GetImageItem()->data.data(), pixels, sizeof(pixels));
    sampled = OIV::Resampler{}.Resample(strip, horizontal ? LLUtils::PointI32{2, 1} : LLUtils::PointI32{1, 2});
    CHECK(ReadPixel(sampled, 0, 0) == 0x10101010);
    CHECK(ReadPixel(sampled, horizontal ? 1 : 0, horizontal ? 0 : 1) == 0x50505050);
}

// Explicitly selected: the original and display buffers together require about 2 GiB.
TEST_CASE("ImageState caps both dimensions of a large image", "[AppCore][ImageState][.][LargeImageMemory]")
{
    auto original = CreateImage(16385, 16385);
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.Refresh();
    auto display = state.GetVisibleImage();
    REQUIRE(state.IsDownscaled());
    CHECK(display->GetDimensions() == LLUtils::Point<uint32_t>{16384, 16384});
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == original);
    CHECK(ReadPixel(display, 0, 0) == 0x80808080);
    CHECK(ReadPixel(display, 16383, 16383) == 0x80808080);
    CheckExtent(state, 16385.0, 16385.0);
}

TEST_CASE("Screen-space detail activates at five display pixels and preserves original texels",
          "[AppCore][ImageState][ScreenSpace]")
{
    auto original = CreateImage(81920, 20);
    WritePixel(original, 99, 4, 0x12345678);
    OIV::ImageState state;
    state.SetScreenSpaceWorkFactor(0.001);
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.SetViewportSize({20, 10});
    state.Refresh();
    state.SetOffset({-100.0, -5.0});
    state.SetScale({0.999, 0.999});
    state.RefreshScreenSpace();
    auto fallback = state.GetVisibleImage();
    CHECK(state.GetVisibleImage() == state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));

    state.SetScale({1.0, 1.0});
    state.Refresh();
    CHECK(state.GetVisibleImage() == fallback);  // Detail is deferred until the idle callback.
    state.RefreshScreenSpace();
    auto detail = state.GetVisibleImage();
    REQUIRE(detail != fallback);
    CHECK(state.GetVisibleImage() != fallback);
    CHECK(state.GetVisibleImage() == detail);
    CHECK(detail->GetDimensions() == LLUtils::Point<uint32_t>{22, 12});
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-1.0, -1.0});
    CHECK(state.GetPresentation().scale == LLUtils::PointF64::One);
    CHECK(ReadPixel(detail, 0, 0) == 0x12345678);
    CheckExtent(state, 81920.0, 20.0);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == detail);
}

TEST_CASE("Screen-space detail downsamples the rasterized region below native zoom",
          "[AppCore][ImageState][ScreenSpace]")
{
    auto original = CreateImage(163840, 20);
    for (int y = 1; y < 11; ++y)
        for (int x = 19; x < 61; ++x)
            WritePixel(original, x, y, static_cast<uint32_t>(x + 8 * y) * 0x01010101);
    OIV::ImageState state;
    state.SetScreenSpaceWorkFactor(0.001);
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.SetViewportSize({20, 4});
    state.SetScale({0.5, 0.5});
    state.Refresh();
    state.SetOffset({-10.0, -1.0});
    state.RefreshScreenSpace();
    auto detail = state.GetVisibleImage();
    REQUIRE(detail != nullptr);
    CHECK(detail->GetDimensions() == LLUtils::Point<uint32_t>{21, 5});
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-0.5, -0.5});
    CHECK(state.GetPresentation().scale == LLUtils::PointF64::One);
    CHECK(ReadPixel(detail, 0, 0) == 0x1f1f1f1f);
    CHECK(ReadPixel(detail, 20, 4) == 0x87878787);
    CheckExtent(state, 81920.0, 10.0);
}

TEST_CASE("Screen-space detail magnifies a crop with the existing rounded pan offset",
          "[AppCore][ImageState][ScreenSpace]")
{
    auto original = CreateImage(32768, 32);
    WritePixel(original, 9, 1, 0xfedcba98);
    OIV::ImageState state;
    state.SetScreenSpaceWorkFactor(0.001);
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.SetViewportSize({25, 20});
    state.SetScale({2.5, 2.5});
    state.Refresh();
    state.SetOffset({-25.4, -5.4});
    state.RefreshScreenSpace();
    auto detail = state.GetVisibleImage();
    REQUIRE(detail != nullptr);
    CHECK(detail->GetDimensions() == LLUtils::Point<uint32_t>{12, 10});
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-2.5, -2.5});
    CHECK(state.GetPresentation().scale == LLUtils::PointF64{2.5, 2.5});
    CHECK(ReadPixel(detail, 0, 0) == 0xfedcba98);
    CheckExtent(state, 81920.0, 80.0);
}

TEST_CASE("Screen-space detail falls back outside coverage and invalidates changed content",
          "[AppCore][ImageState][ScreenSpace]")
{
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(81920, 20)});
    state.SetViewportSize({20, 10});
    state.RefreshScreenSpace();
    auto detail = state.GetVisibleImage();
    REQUIRE(detail != nullptr);
    auto fallback = state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled);

    state.SetOffset({-100.0, -5.0});
    CHECK(state.GetVisibleImage() == fallback);
    CHECK(state.GetVisibleImage() != detail);
    state.Refresh();
    CHECK(state.GetVisibleImage() == state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    state.RefreshScreenSpace();
    REQUIRE(state.GetVisibleImage() != detail);
    detail = state.GetVisibleImage();

    state.SetScale({1.5, 1.5});
    CHECK(state.GetVisibleImage() != detail);
    CHECK(state.GetVisibleImage() == fallback);
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    REQUIRE(detail != fallback);
    CheckExtent(state, 122880.0, 30.0);

    state.SetViewportSize({40, 20});
    CHECK(state.GetVisibleImage() != detail);
    CHECK(state.GetVisibleImage() == fallback);
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    REQUIRE(detail != fallback);

    state.SetResample(true);
    CHECK(state.GetVisibleImage() == detail);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == detail);
    state.SetUseRainbowNormalization(true);
    CHECK(state.GetVisibleImage() != detail);
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    REQUIRE(detail != state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));

    state.SetOffset({0.0, 0.0});
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    state.Transform(IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::None);
    CHECK(state.GetVisibleImage() != detail);
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    REQUIRE(detail != state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    CheckExtent(state, 30.0, 122880.0);

    state.SetSelectedSource(CreateImage(16, 16));
    state.ResetUserState();
    CHECK(state.GetVisibleImage() != detail);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    CHECK_FALSE(state.IsDownscaled());

    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(81920, 20)});
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    state.ClearAll();
    CHECK(state.GetVisibleImage() != detail);
    CHECK(state.GetVisibleImage() == nullptr);
}

TEST_CASE("Screen-space detail clips to image edges and handles empty views", "[AppCore][ImageState][ScreenSpace]")
{
    const auto size = GENERATE(LLUtils::PointI32{81920, 1}, LLUtils::PointI32{1, 81920});
    OIV::ImageState state;
    state.SetScreenSpaceWorkFactor(0.001);
    auto original = CreateImage(size.x, size.y);
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.SetViewportSize({20, 20});
    state.RefreshScreenSpace();
    auto detail = state.GetVisibleImage();
    REQUIRE(detail != nullptr);
    CHECK(detail->GetWidth() == std::min(size.x, 21));
    CHECK(detail->GetHeight() == std::min(size.y, 21));

    state.SetOffset({20.0 - size.x, 20.0 - size.y});
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    REQUIRE(detail != nullptr);
    CHECK(detail->GetWidth() == std::min(size.x, 21));
    CHECK(detail->GetHeight() == std::min(size.y, 21));
    CHECK(ReadPixel(detail, detail->GetWidth() - 1, detail->GetHeight() - 1) == 0x80808080);

    for (const auto offset : {LLUtils::PointF64{21.0, 0.0}, LLUtils::PointF64{-size.x - 1.0, 0.0},
                              LLUtils::PointF64{0.0, 21.0}, LLUtils::PointF64{0.0, -size.y - 1.0}})
    {
        state.SetOffset(offset);
        state.RefreshScreenSpace();
        CHECK(state.GetVisibleImage() == state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    }

    state.SetOffset({0.0, 0.0});
    state.SetViewportSize({0, 0});
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    CHECK(state.GetVisibleImage() != detail);
}

TEST_CASE("Screen-space detail still obeys the texture limit on an oversized viewport",
          "[AppCore][ImageState][ScreenSpace]")
{
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(81920, 20)});
    state.SetViewportSize({30000, 30});
    state.RefreshScreenSpace();
    auto detail = state.GetVisibleImage();
    REQUIRE(detail != nullptr);
    CHECK(detail->GetWidth() == 16384);
    CHECK(detail->GetHeight() == 20);
    CHECK(state.GetPresentation().scale.x == Catch::Approx(30001.0 / 16384.0));
    CheckExtent(state, 81920.0, 20.0);
}

TEST_CASE("Region resampling reads original row pitch and copies native texels exactly",
          "[AppCore][ImageState][Resampler]")
{
    auto original                    = CreateImage(8, 6);
    auto& item                       = original->GetImageItem();
    item->descriptor.rowPitchInBytes = 8 * 4 + 3;
    item->data.Allocate(item->descriptor.rowPitchInBytes * 6);
    std::fill_n(item->data.data(), item->data.Size(), std::byte{0xff});
    for (int y = 0; y < 6; ++y)
        for (int x = 0; x < 8; ++x)
            WritePixel(original, x, y, static_cast<uint32_t>(x + 10 * y) * 0x01010101);

    auto crop = OIV::Resampler{}.Resample(original, {3, 2}, LLUtils::RectI32{{2, 1}, {5, 3}});
    CHECK(ReadPixel(crop, 0, 0) == 0x0c0c0c0c);
    CHECK(ReadPixel(crop, 2, 1) == 0x18181818);
    auto reduced = OIV::Resampler{}.Resample(original, {2, 2}, LLUtils::RectI32{{2, 1}, {6, 5}});
    CHECK(ReadPixel(reduced, 0, 0) == 0x11111111);
    CHECK(ReadPixel(reduced, 1, 1) == 0x27272727);

    reduced = OIV::Resampler{}.Resample(original, {4, 3});
    CHECK(ReadPixel(reduced, 0, 0) == 0x05050505);
    CHECK(ReadPixel(reduced, 3, 2) == 0x33333333);
    crop = OIV::Resampler{}.Resample(original, {8, 6});
    CHECK(ReadPixel(crop, 7, 5) == ReadPixel(original, 7, 5));
    CHECK(crop->GetRowPitchInBytes() == 8 * 4);

    CHECK_THROWS(OIV::Resampler{}.Resample(original, {2, 2}, LLUtils::RectI32{{-1, 0}, {3, 3}}));
    CHECK_THROWS(OIV::Resampler{}.Resample(original, {2, 2}, LLUtils::RectI32{{7, 0}, {9, 3}}));
    CHECK_THROWS(OIV::Resampler{}.Resample(original, {0, 2}));
}

TEST_CASE("Screen-space margins follow the viewport work budget", "[AppCore][ImageState][ScreenSpace]")
{
    struct Case
    {
        double zoom;
        double factor;
        uint32_t width;
        uint32_t height;
    };
    const auto test = GENERATE(Case{0.25, 16.0, 17, 9}, Case{0.5, 4.0, 17, 9}, Case{0.5, 9.0, 25, 13},
                               Case{0.5, 16.0, 33, 17}, Case{1.0, 16.0, 34, 18});
    OIV::ImageState state;
    state.SetScreenSpaceWorkFactor(test.factor);
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(327680, 64)});
    state.SetViewportSize({16, 8});
    state.SetScale({test.zoom, test.zoom});
    state.Refresh();
    state.SetOffset({-1000.0, -6.0});
    state.RefreshScreenSpace();
    const auto detail = state.GetVisibleImage();
    REQUIRE(detail != nullptr);
    CHECK(detail->GetWidth() == test.width);
    CHECK(detail->GetHeight() == test.height);
    CheckExtent(state, 327680 * test.zoom, 64 * test.zoom);
}

TEST_CASE("Screen-space work defaults scale with viewport size and settings wait for regeneration",
          "[AppCore][ImageState][ScreenSpace]")
{
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(163840, 128)});
    state.SetViewportSize({16, 8});
    state.SetScale({0.5, 0.5});
    state.Refresh();
    state.SetOffset({-1000.0, -16.0});
    state.RefreshScreenSpace();
    auto detail = state.GetVisibleImage();
    CHECK(detail->GetDimensions() == LLUtils::Point<uint32_t>{33, 17});
    state.SetScreenSpaceWorkFactor(4.0);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == detail);

    state.SetOffset({-2000.0, -16.0});
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    CHECK(detail->GetDimensions() == LLUtils::Point<uint32_t>{17, 9});

    state.SetScreenSpaceWorkFactor(16.0);
    state.SetViewportSize({32, 16});
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    CHECK(detail->GetDimensions() == LLUtils::Point<uint32_t>{65, 33});
}

TEST_CASE("Screen-space detail reuses source coordinates across pan resize and native magnification",
          "[AppCore][ImageState][ScreenSpace]")
{
    auto original = CreateImage(81920, 64);
    WritePixel(original, 89, 10, 0x12345678);
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.SetViewportSize({20, 10});
    state.Refresh();
    state.SetOffset({-100.0, -16.0});
    state.RefreshScreenSpace();
    auto detail   = state.GetVisibleImage();
    auto fallback = state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled);
    CHECK(detail->GetDimensions() == LLUtils::Point<uint32_t>{42, 22});
    CHECK(ReadPixel(detail, 0, 0) == 0x12345678);
    const auto pixels  = detail;
    const auto* buffer = pixels->GetBuffer();

    state.SetOffset({-104.4, -16.0});
    CHECK(state.GetVisibleImage() == detail);
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-15.0, -6.0});
    CHECK(detail->GetBuffer() == buffer);
    CHECK(state.GetVisibleImage() != fallback);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == detail);

    state.SetViewportSize({24, 10});
    CHECK(state.GetVisibleImage() == detail);
    state.SetOffset({-500.0, -16.0});
    CHECK(state.GetVisibleImage() != detail);
    CHECK(state.GetVisibleImage() == fallback);
    CHECK(state.GetVisibleImage() == state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    state.SetOffset({-100.0, -16.0});
    CHECK(state.GetVisibleImage() == detail);

    // Keep the view inside the same source crop while magnifying its original texels.
    state.SetScale({2.0, 2.0});
    state.SetOffset({-200.0, -32.0});
    CHECK(state.GetVisibleImage() == detail);
    CHECK(state.GetPresentation().scale == LLUtils::PointF64{2.0, 2.0});
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-22.0, -12.0});
    state.SetResample(true);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == detail);
    state.SetResample(false);
    CHECK(state.GetVisibleImage() == detail);
    CheckExtent(state, 163840.0, 128.0);
}

TEST_CASE("Screen-space zoom tolerance uses rounded sample counts and preserves native texels",
          "[AppCore][ImageState][ScreenSpace]")
{
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(327680, 128)});
    state.SetViewportSize({40, 16});
    state.SetScale({0.5, 0.5});
    state.Refresh();
    state.SetOffset({-1000.0, -16.0});
    state.RefreshScreenSpace();
    auto detail   = state.GetVisibleImage();
    auto fallback = state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled);
    REQUIRE(detail != fallback);

    const auto useZoom = [&](double zoom)
    {
        state.SetScale({zoom, zoom});
        state.SetOffset({-2000.0 * zoom, -32.0 * zoom});
    };
    useZoom(0.6);
    CHECK(state.GetVisibleImage() == detail);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == detail);

    state.SetScreenSpaceZoomTolerance(0.0);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() != detail);
    detail = state.GetVisibleImage();
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == detail);  // Integer rounding must not cause endless regeneration.

    state.SetScreenSpaceZoomTolerance(0.25);
    useZoom(0.5);  // Inside the lower tolerance bound relative to the 60% crop.
    CHECK(state.GetVisibleImage() == detail);
    useZoom(0.45);
    CHECK(state.GetVisibleImage() == fallback);
    useZoom(0.72);
    CHECK(state.GetVisibleImage() == detail);
    useZoom(0.8);
    CHECK(state.GetVisibleImage() == fallback);

    state.SetScreenSpaceZoomTolerance(10.0);
    useZoom(1.0);
    CHECK(state.GetVisibleImage() == fallback);  // Even a generous tolerance cannot substitute reduced pixels at 100%.
    state.RefreshScreenSpace();
    const auto native = state.GetVisibleImage();
    CHECK(native != detail);
    CHECK(state.GetPresentation().scale == LLUtils::PointF64::One);
}

TEST_CASE("Screen-space detail memory target limits margins without degrading the mandatory crop",
          "[AppCore][ImageState][ScreenSpace][.][LargeImageMemory]")
{
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(16385, 16385)});
    state.SetScale({5.0, 5.0});
    state.SetViewportSize({30000, 10000});
    state.RefreshScreenSpace();
    auto detail = state.GetVisibleImage();
    REQUIRE(detail != state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    const uint64_t bytes = static_cast<uint64_t>(detail->GetWidth()) * detail->GetHeight() * 4;
    CHECK(bytes <= 64 * 1024 * 1024);
    CHECK(detail->GetWidth() > 6001);
    CHECK(detail->GetHeight() > 2001);
    CHECK(state.GetPresentation().scale == LLUtils::PointF64{5.0, 5.0});

    state.SetViewportSize({45000, 15000});
    state.RefreshScreenSpace();
    detail = state.GetVisibleImage();
    CHECK(detail->GetDimensions() == LLUtils::Point<uint32_t>{9001, 3001});
    CHECK(state.GetPresentation().scale == LLUtils::PointF64{5.0, 5.0});
}

TEST_CASE("Pixel grid follows the visible detail layer rather than retained fallback content",
          "[AppCore][ImageState][ScreenSpace]")
{
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(16, 16)});
    state.Refresh();
    CHECK(state.GetPresentation().canShowPixelGrid);

    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(81920, 64)});
    state.SetViewportSize({20, 10});
    state.Refresh();
    CHECK_FALSE(state.GetPresentation().canShowPixelGrid);

    state.RefreshScreenSpace();
    const auto detail = state.GetVisibleImage();
    CHECK(state.GetPresentation().canShowPixelGrid);
    state.SetOffset({-3.0, 0.0});
    CHECK(state.GetVisibleImage() == detail);
    CHECK(state.GetPresentation().canShowPixelGrid);

    state.SetOffset({-200.0, 0.0});
    CHECK(state.GetVisibleImage() == state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    CHECK(state.GetVisibleImage() != detail);
    CHECK_FALSE(state.GetPresentation().canShowPixelGrid);
    state.SetOffset({0.0, 0.0});
    CHECK(state.GetVisibleImage() == detail);
    CHECK(state.GetPresentation().canShowPixelGrid);

    state.SetScale({0.1, 0.1});
    CHECK_FALSE(state.GetPresentation().canShowPixelGrid);
    state.SetScale({1.0, 1.0});
    CHECK(state.GetVisibleImage() == detail);
    CHECK(state.GetPresentation().canShowPixelGrid);

    state.SetViewportSize({200, 100});
    CHECK_FALSE(state.GetPresentation().canShowPixelGrid);
    state.RefreshScreenSpace();
    CHECK(state.GetPresentation().canShowPixelGrid);

    state.SetUseRainbowNormalization(true);
    CHECK_FALSE(state.GetPresentation().canShowPixelGrid);
    state.RefreshScreenSpace();
    CHECK(state.GetPresentation().canShowPixelGrid);

    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(16, 16)});
    state.Refresh();
    CHECK(state.GetPresentation().canShowPixelGrid);
}

TEST_CASE("ImageState preserves current rounded presentation offsets through refinement", "[AppCore][ImageState]")
{
    OIV::ImageState state;
    state.SetScreenSpaceWorkFactor(0.001);
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(32768, 32)});
    state.SetViewportSize({25, 20});
    state.SetScale({2.5, 2.5});
    state.SetOffset({-25.4, -5.4});
    state.RefreshScreenSpace();
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-2.5, -2.5});
    state.SetResample(true);
    state.RefreshScreenSpace();
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-2.5, -2.5});
    state.SetOffset({-26.4, -6.4});
    // Moving beyond the filtering border selects the fallback until idle refinement covers the new view.
    CHECK(state.GetVisibleImage() == state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-26.0, -6.0});
    state.RefreshScreenSpace();
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-3.5, -3.5});
    state.SetResample(false);
    CHECK(state.GetPresentation().offset == LLUtils::PointF64{-3.5, -3.5});
}

TEST_CASE("ImageState observation never processes pixels", "[AppCore][ImageState]")
{
    auto original = CreateImage(8, 4);
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    CHECK(state.GetVisibleImage() == nullptr);
    CheckExtent(state, 8, 4);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::RenderFormat) == nullptr);
    state.Refresh();
    CHECK(state.GetVisibleImage() == original);
    state.SetScale({0.5, 0.5});
    state.SetResample(true);
    CheckExtent(state, 4, 2);
    CHECK(state.GetVisibleImage() == original);
    state.Refresh();
    CHECK(state.GetVisibleImage()->GetDimensions() == LLUtils::Point<uint32_t>{4, 2});
}

TEST_CASE("ImageState preserves rotation and flip composition", "[AppCore][ImageState]")
{
    auto original = CreateImage(2, 3);
    for (int y = 0; y < 3; ++y)
        for (int x = 0; x < 2; ++x)
            WritePixel(original, x, y, static_cast<uint32_t>(1 + x + 2 * y));
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.Transform(IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::None);
    state.Refresh(OIV::ImageProcessingStage::GeometricTransform);
    auto rotated = state.GetRetainedImage(OIV::ImageProcessingStage::GeometricTransform);
    CHECK(rotated->GetDimensions() == LLUtils::Point<uint32_t>{3, 2});
    CHECK(ReadPixel(rotated, 0, 0) == 5);
    CHECK(ReadPixel(rotated, 2, 1) == 2);
    state.Transform(IMUtil::AxisAlignedRotation::None, IMUtil::AxisAlignedFlip::Horizontal);
    CHECK(state.GetAxisAlignedRotation() == IMUtil::AxisAlignedRotation::Rotate90CCW);
    CHECK(state.GetAxisAlignedFlip() == IMUtil::AxisAlignedFlip::Vertical);
    state.Refresh(OIV::ImageProcessingStage::GeometricTransform);
    rotated = state.GetRetainedImage(OIV::ImageProcessingStage::GeometricTransform);
    CHECK(ReadPixel(rotated, 0, 0) == 1);
    CHECK(ReadPixel(rotated, 2, 1) == 6);
    state.ResetUserState();
    for (int turn = 0; turn < 4; ++turn)
        state.Transform(IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::None);
    state.Refresh(OIV::ImageProcessingStage::GeometricTransform);
    CHECK(state.GetRetainedImage(OIV::ImageProcessingStage::GeometricTransform) == original);
}

TEST_CASE("ImageState retains hidden viewport pixels but releases them when content changes", "[AppCore][ImageState]")
{
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(81920, 32)});
    state.SetViewportSize({20, 10});
    state.RefreshScreenSpace();
    std::weak_ptr<IMCodec::Image> cached = state.GetVisibleImage();
    const auto* pixels                   = state.GetVisibleImage()->GetBuffer();
    state.SetOffset({-500.0, 0.0});
    CHECK_FALSE(cached.expired());
    CHECK(state.GetVisibleImage() != cached.lock());
    state.SetOffset({0.0, 0.0});
    CHECK(state.GetVisibleImage() == cached.lock());
    CHECK(state.GetVisibleImage()->GetBuffer() == pixels);
    state.SetUseRainbowNormalization(true);
    CHECK(cached.expired());
    state.RefreshScreenSpace();
    cached = state.GetVisibleImage();
    state.ClearAll();
    CHECK(cached.expired());
}

TEST_CASE("Screen-space buffer targets preserve caches and threshold changes reevaluate presentation",
          "[AppCore][ImageState][ScreenSpace]")
{
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(163840, 128)});
    state.SetViewportSize({16, 8});
    state.SetScale({0.5, 0.5});
    state.Refresh();
    state.SetOffset({-1000.0, -16.0});
    const auto fallback = state.GetVisibleImage();
    state.RefreshScreenSpace();
    const auto cached = state.GetVisibleImage();
    REQUIRE(cached != fallback);
    CHECK(cached->GetDimensions() == LLUtils::Point<uint32_t>{33, 17});

    state.SetScreenSpacePixelBufferTargetBytes(1);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == cached);
    state.SetScreenSpaceTexelThresholdPixels(5.1);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == fallback);
    CHECK_FALSE(state.GetPresentation().canShowPixelGrid);
    state.SetScreenSpaceTexelThresholdPixels(5.0);
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() == cached);
    CHECK(state.GetPresentation().canShowPixelGrid);

    state.SetOffset({-2000.0, -16.0});
    state.RefreshScreenSpace();
    CHECK(state.GetVisibleImage() != cached);
    CHECK(state.GetVisibleImage()->GetDimensions() == LLUtils::Point<uint32_t>{17, 9});
}

TEST_CASE("Resampler includes every texel at odd reduction ratios", "[AppCore][ImageState][Resampler]")
{
    const bool vertical = GENERATE(false, true);
    auto original       = CreateImage(vertical ? 1 : 6, vertical ? 6 : 1);
    const uint32_t pixels[]{0, 0, 0xffffffff, 0, 0, 0xffffffff};
    std::memcpy(original->GetImageItem()->data.data(), pixels, sizeof(pixels));
    const auto sampled = OIV::Resampler{}.Resample(original,
                                                   vertical ? LLUtils::PointI32{1, 2} : LLUtils::PointI32{2, 1});
    CHECK(ReadPixel(sampled, 0, 0) == 0x55555555);
    CHECK(ReadPixel(sampled, vertical ? 0 : 1, vertical ? 1 : 0) == 0x55555555);
}

TEST_CASE("Resampler weights fractional footprints across padded crop rows", "[AppCore][ImageState][Resampler]")
{
    auto original                    = CreateImage(7, 7);
    auto& item                       = original->GetImageItem();
    item->descriptor.rowPitchInBytes = 7 * 4 + 3;
    item->data.Allocate(item->descriptor.rowPitchInBytes * 7);
    std::fill_n(item->data.data(), item->data.Size(), std::byte{0xff});
    for (int y = 1; y < 6; ++y)
        for (int x = 1; x < 6; ++x)
            WritePixel(original, x, y, 0);
    WritePixel(original, 3, 3, 0xffffffff);
    const auto sampled = OIV::Resampler{}.Resample(original, {2, 2}, LLUtils::RectI32{{1, 1}, {6, 6}});
    // Each 2.5 x 2.5 footprint receives one quarter of the center texel: floor(255 * 0.25 / 6.25) = 10.
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 2; ++x)
            CHECK(ReadPixel(sampled, x, y) == 0x0a0a0a0a);
}

TEST_CASE("Resampler shares fractional boundary texels without losing channels", "[AppCore][ImageState][Resampler]")
{
    const bool vertical = GENERATE(false, true);
    auto original       = CreateImage(vertical ? 1 : 5, vertical ? 5 : 1);
    const uint32_t pixels[]{0, 0, 0xff804020, 0, 0};
    std::memcpy(original->GetImageItem()->data.data(), pixels, sizeof(pixels));
    const auto sampled = OIV::Resampler{}.Resample(original,
                                                   vertical ? LLUtils::PointI32{1, 2} : LLUtils::PointI32{2, 1});
    // The middle source texel contributes half its area to each 2.5-texel output footprint.
    CHECK(ReadPixel(sampled, 0, 0) == 0x33190c06);
    CHECK(ReadPixel(sampled, vertical ? 0 : 1, vertical ? 1 : 0) == 0x33190c06);
}

TEST_CASE("Large-image fallback preserves features at a three-to-one reduction", "[AppCore][ImageState][Resampler]")
{
    auto original = CreateImage(49152, 1);
    for (int x = 0; x < 49152; ++x)
        WritePixel(original, x, 0, x % 3 == 2 ? 0xffffffff : 0);
    OIV::ImageState state;
    state.SetOpenedImage(OIV::ViewerImage{.pixels = original});
    state.Refresh();
    const auto fallback = state.GetVisibleImage();
    REQUIRE(state.IsDownscaled());
    REQUIRE(fallback->GetDimensions() == LLUtils::Point<uint32_t>{16384, 1});
    for (int x = 0; x < 16384; ++x)
        if (ReadPixel(fallback, x, 0) != 0x55555555)
        {
            CAPTURE(x);
            FAIL("A bright source texel was lost from the fallback");
        }
}

TEST_CASE("Resampler accumulates large footprints without channel overflow", "[AppCore][ImageState][Resampler]")
{
    // A full-white footprint this size exceeds a 32-bit channel sum.
    auto original = CreateImage(4097, 4113);
    auto& data    = original->GetImageItem()->data;
    std::fill_n(data.data(), data.Size(), std::byte{0xff});
    const auto sampled = OIV::Resampler{}.Resample(original, {1, 1});
    CHECK(ReadPixel(sampled, 0, 0) == 0xffffffff);
}

TEST_CASE("Resampler prepared boundaries preserve exact coverage across crops and worker rows",
          "[AppCore][ImageState][Resampler]")
{
    const auto target = GENERATE(LLUtils::PointI32{13, 11}, LLUtils::PointI32{1, 1}, LLUtils::PointI32{1, 9},
                                 LLUtils::PointI32{9, 1}, LLUtils::PointI32{35, 31}, LLUtils::PointI32{47, 43});
    auto original     = CreateImage(41, 37);
    auto& item        = original->GetImageItem();
    item->descriptor.rowPitchInBytes = 41 * 4 + 3;
    item->data.Allocate(item->descriptor.rowPitchInBytes * 37);
    std::mt19937 random(1729);
    for (int y = 0; y < 37; ++y)
        for (int x = 0; x < 41; ++x)
            WritePixel(original, x, y, random());

    constexpr int OriginX      = 3;
    constexpr int OriginY      = 2;
    constexpr int SourceWidth  = 35;
    constexpr int SourceHeight = 31;
    const auto sampled         = OIV::Resampler{}.Resample(
        original, target, LLUtils::RectI32{{OriginX, OriginY}, {OriginX + SourceWidth, OriginY + SourceHeight}});
    for (int y = 0; y < target.y; ++y)
        for (int x = 0; x < target.x; ++x)
        {
            // Scan all source cells independently of the production boundary tables. Integer intersections
            // provide an exact reference, including fractional footprints contained within one source texel.
            std::array<uint64_t, 4> sum{};
            const uint64_t left   = static_cast<uint64_t>(x) * SourceWidth;
            const uint64_t right  = left + SourceWidth;
            const uint64_t top    = static_cast<uint64_t>(y) * SourceHeight;
            const uint64_t bottom = top + SourceHeight;
            for (int sourceY = 0; sourceY < SourceHeight; ++sourceY)
            {
                const auto firstY = std::max(top, static_cast<uint64_t>(sourceY) * target.y);
                const auto lastY  = std::min(bottom, static_cast<uint64_t>(sourceY + 1) * target.y);
                if (lastY > firstY)
                {
                    for (int sourceX = 0; sourceX < SourceWidth; ++sourceX)
                    {
                        const auto firstX = std::max(left, static_cast<uint64_t>(sourceX) * target.x);
                        const auto lastX  = std::min(right, static_cast<uint64_t>(sourceX + 1) * target.x);
                        if (lastX > firstX)
                        {
                            const uint64_t weight = (lastX - firstX) * (lastY - firstY);
                            const auto* pixel     = reinterpret_cast<const uint8_t*>(
                                original->GetBufferAt(OriginX + sourceX, OriginY + sourceY));
                            for (size_t channel = 0; channel < sum.size(); ++channel)
                                sum[channel] += pixel[channel] * weight;
                        }
                    }
                }
            }
            std::array<uint8_t, 4> expected{};
            for (size_t channel = 0; channel < sum.size(); ++channel)
                expected[channel] = static_cast<uint8_t>(sum[channel] / (SourceWidth * SourceHeight));
            CAPTURE(target.x, target.y, x, y);
            CHECK(ReadPixel(sampled, x, y) == std::bit_cast<uint32_t>(expected));
        }
}

TEST_CASE("ImageState presents one coherent selection with its geometry and pixel grid",
          "[AppCore][ImageState][ScreenSpace]")
{
    OIV::ImageState state;
    const auto empty = state.GetPresentation();
    CHECK(empty.image == nullptr);
    CHECK_FALSE(empty.canShowPixelGrid);

    state.SetOpenedImage(OIV::ViewerImage{.pixels = CreateImage(81920, 64)});
    state.SetViewportSize({20, 10});
    state.SetOffset({-100.4, -16.4});
    state.Refresh();
    const auto fallback = state.GetPresentation();
    CHECK(fallback.image == state.GetRetainedImage(OIV::ImageProcessingStage::Downscaled));
    CHECK(fallback.offset == LLUtils::PointF64{-100, -16});
    CHECK(fallback.scale.x == Catch::Approx(5.0));
    CHECK_FALSE(fallback.canShowPixelGrid);

    state.RefreshScreenSpace();
    const auto detail = state.GetPresentation();
    CHECK(detail.image == state.GetCachedScreenSpaceImage());
    CHECK(detail.image != fallback.image);
    CHECK(detail.offset == LLUtils::PointF64{-11, -6});
    CHECK(detail.scale == LLUtils::PointF64::One);
    CHECK(detail.canShowPixelGrid);

    state.SetOffset({-500, -16});
    const auto moved = state.GetPresentation();
    CHECK(moved.image == fallback.image);
    CHECK(moved.offset == LLUtils::PointF64{-500, -16});
    CHECK_FALSE(moved.canShowPixelGrid);
    // Previously captured geometry remains tied to its selected pixels after the view changes.
    CHECK(detail.offset == LLUtils::PointF64{-11, -6});
    CHECK(state.GetCachedScreenSpaceImage() == detail.image);
}
