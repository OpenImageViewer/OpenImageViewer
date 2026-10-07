#include <catch2/catch_all.hpp>

#include <ImageLoader.h>
#include <ImageUtil/ImageUtil.h>
#include <LWS/Cursor.hpp>

#include <array>
#include <filesystem>

TEST_CASE("Auto-scroll cursor resources decode into portable cursors", "[oiviewer][cursor]")
{
    constexpr std::array names{"ArrowC.cur", "ArrowE.cur",  "ArrowNE.cur", "ArrowN.cur", "ArrowNW.cur",
                               "ArrowW.cur", "ArrowSW.cur", "ArrowS.cur",  "ArrowSE.cur"};
    const auto folder = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() / "Clients" / "OIViewer" /
                        "Resources" / "Cursors";
    IMCodec::ImageLoader loader;
    for (const auto name : names)
    {
        INFO(name);
        IMCodec::ImageSharedPtr image;
        REQUIRE(loader.Decode((folder / name).native(), IMCodec::ImageLoadFlags::None, {},
                              IMCodec::PluginTraverseMode::AnyFileType, image) == IMCodec::ImageResult::Success);
        REQUIRE(image != nullptr);
        image = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8, false);
        REQUIRE(image != nullptr);
        REQUIRE(image->GetWidth() == 32);
        REQUIRE(image->GetHeight() == 32);
        const LWS::BitmapBuffer bitmap{
            .pixels   = {image->GetBuffer(), static_cast<size_t>(image->GetRowPitchInBytes()) * image->GetHeight()},
            .format   = LWS::BitmapPixelFormat::Bgra8,
            .rowOrder = LWS::BitmapRowOrder::TopDown,
            .width    = image->GetWidth(),
            .height   = image->GetHeight(),
            .rowPitch = image->GetRowPitchInBytes(),
        };
        REQUIRE(LWS::Cursor::FromBitmap(bitmap, {16, 16}).has_value());
    }
}
