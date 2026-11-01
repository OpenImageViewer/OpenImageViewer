#include <catch2/catch_test_macros.hpp>

#include <OIVShared/PixelHelper.h>

#include <array>
#include <cstddef>
#include <memory>

TEST_CASE("PixelHelper counts packed multi-byte texels", "[Shared][PixelHelper]")
{
    auto imageItem                                = std::make_shared<IMCodec::ImageItem>();
    imageItem->itemType                           = IMCodec::ImageItemType::Image;
    imageItem->descriptor.width                   = 3;
    imageItem->descriptor.height                  = 1;
    imageItem->descriptor.rowPitchInBytes         = 9;
    imageItem->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8;
    imageItem->descriptor.texelFormatStorage      = IMCodec::TexelFormat::I_R8_G8_B8;
    const std::array pixels{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4}, std::byte{5},
        std::byte{6}, std::byte{1}, std::byte{2}, std::byte{3},
    };
    imageItem->data.Allocate(pixels.size());
    imageItem->data.Write(pixels.data(), 0, pixels.size());
    const auto image = std::make_shared<IMCodec::Image>(imageItem, IMCodec::ImageItemType::Unknown);

    REQUIRE(OIV::PixelHelper::CountUniqueValues(image) == 2);
}
