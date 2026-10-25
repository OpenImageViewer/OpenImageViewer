#ifdef _WIN32
    #include <catch2/catch_test_macros.hpp>
    #include "SrcPlatform/Win32/WlxPreview.h"
    #include <ImageLoader.h>
    #include <ImageUtil/ImageUtil.h>
    #include <future>
    #include <latch>
    #include <array>
    #include <filesystem>
    #include <fstream>
    #include <limits>
    #include <memory>
    #include <vector>

namespace
{
    struct TempImage
    {
        explicit TempImage(const wchar_t* extension)
        {
            static unsigned sequence{};
            folder = std::filesystem::temp_directory_path() /
                     (L"oiv-preview-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                      std::to_wstring(GetTickCount64()) + L"-" + std::to_wstring(sequence++));
            REQUIRE(std::filesystem::create_directory(folder));
            file = folder / (std::wstring(L"image-\u05d0") + extension);
        }
        ~TempImage()
        {
            std::error_code error;
            std::filesystem::remove(file, error);
            std::filesystem::remove(folder, error);
        }
        void Write(const void* data, size_t size)
        {
            std::ofstream stream(file, std::ios::binary);
            stream.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
            REQUIRE(stream.good());
        }
        std::filesystem::path folder, file;
    };

    struct DeleteBitmap
    {
        void operator()(HBITMAP value) const { DeleteObject(value); }
    };
    using Bitmap = std::unique_ptr<std::remove_pointer_t<HBITMAP>, DeleteBitmap>;

    DIBSECTION Pixels(HBITMAP bitmap)
    {
        DIBSECTION result{};
        REQUIRE(bitmap != nullptr);
        REQUIRE(GetObjectW(bitmap, sizeof(result), &result) == sizeof(result));
        REQUIRE(result.dsBm.bmBits != nullptr);
        REQUIRE(result.dsBm.bmBitsPixel == 32);
        // GetObject normalizes the DIB height; row colors below verify the actual pixel order.
        return result;
    }

    void WriteBmp(TempImage& file)
    {
        BITMAPFILEHEADER header{};
        header.bfType    = 0x4D42;
        header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
        header.bfSize    = header.bfOffBits + 24;
        BITMAPINFOHEADER info{};
        info.biSize     = sizeof(info);
        info.biWidth    = 3;
        info.biHeight   = 2;
        info.biPlanes   = 1;
        info.biBitCount = 24;
        // Bottom-up BGR rows with padding: blue on the bottom, red on the top.
        constexpr unsigned char pixels[]{255, 0, 0,   255, 0, 0,   255, 0, 0,   0, 0, 0,
                                         0,   0, 255, 0,   0, 255, 0,   0, 255, 0, 0, 0};
        std::ofstream stream(file.file, std::ios::binary);
        stream.write(reinterpret_cast<const char*>(&header), sizeof(header));
        stream.write(reinterpret_cast<const char*>(&info), sizeof(info));
        stream.write(reinterpret_cast<const char*>(pixels), sizeof(pixels));
        REQUIRE(stream.good());
    }
}  // namespace

TEST_CASE("WLX previews decode Unicode paths into correctly oriented host-owned pixels", "[wlx][preview]")
{
    IMCodec::ImageLoader loader;
    TempImage file(L".bmp");
    WriteBmp(file);
    Bitmap bitmap(OIV::Win32::CreatePreviewBitmap(loader, file.file.c_str(), 3, 2));
    auto dib = Pixels(bitmap.get());
    REQUIRE(dib.dsBm.bmWidth == 3);
    REQUIRE(dib.dsBm.bmHeight == 2);
    const auto* pixels = static_cast<const uint32_t*>(dib.dsBm.bmBits);
    REQUIRE(pixels[0] == 0xFFFF0000);
    REQUIRE(pixels[3] == 0xFF0000FF);

    Bitmap padded(OIV::Win32::CreatePreviewBitmap(loader, file.file.c_str(), 5, 4));
    auto padding             = Pixels(padded.get());
    const auto* paddedPixels = static_cast<const uint32_t*>(padding.dsBm.bmBits);
    REQUIRE(paddedPixels[0] == 0xFFFFFFFF);
    REQUIRE(paddedPixels[6] == 0xFFFF0000);
    REQUIRE(paddedPixels[11] == 0xFF0000FF);
    REQUIRE(paddedPixels[19] == 0xFFFFFFFF);
}

TEST_CASE("WLX previews composite transparency onto white", "[wlx][preview]")
{
    constexpr unsigned char png[]{0x89, 0x50, 0x4e, 0x47, 0xd,  0xa,  0x1a, 0xa,  0,    0,    0,    0xd,
                                  0x49, 0x48, 0x44, 0x52, 0,    0,    0,    2,    0,    0,    0,    1,
                                  8,    6,    0,    0,    0,    0xf4, 0x22, 0x7f, 0x8a, 0,    0,    0,
                                  0xf,  0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0,    2,
                                  0xff, 0xff, 3,    0,    0xa,  0xfe, 2,    0xfe, 0x5f, 0x2d, 0x82, 0x35,
                                  0,    0,    0,    0,    0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
    TempImage file(L".png");
    file.Write(png, sizeof(png));
    IMCodec::ImageLoader loader;
    Bitmap bitmap(OIV::Win32::CreatePreviewBitmap(loader, file.file.c_str(), 2, 1));
    auto dib           = Pixels(bitmap.get());
    const auto* pixels = static_cast<const uint32_t*>(dib.dsBm.bmBits);
    REQUIRE(pixels[0] == 0xFFFFFFFF);
    REQUIRE(pixels[1] == 0xFF0000FF);
}

TEST_CASE("WLX previews select the first animation frame", "[wlx][preview]")
{
    constexpr unsigned char gif[]{'G',  'I',  'F', '8', '9', 'a',  2,    0,    1,    0,   0x80, 0, 0,    255,
                                  0,    0,    0,   0,   255, 0x21, 0xf9, 4,    0,    10,  0,    0, 0,    0x2c,
                                  0,    0,    0,   0,   2,   0,    1,    0,    0,    2,   2,    4, 0x0a, 0,
                                  0x21, 0xf9, 4,   0,   10,  0,    0,    0,    0x2c, 0,   0,    0, 0,    2,
                                  0,    1,    0,   0,   2,   2,    0x4c, 0x0a, 0,    0x3b};
    TempImage file(L".gif");
    file.Write(gif, sizeof(gif));
    IMCodec::ImageLoader loader;
    Bitmap bitmap(OIV::Win32::CreatePreviewBitmap(loader, file.file.c_str(), 2, 1));
    auto dib           = Pixels(bitmap.get());
    const auto* pixels = static_cast<const uint32_t*>(dib.dsBm.bmBits);
    REQUIRE(pixels[0] == 0xFFFF0000);
    REQUIRE(pixels[1] == 0xFFFF0000);
}

TEST_CASE("WLX preview failures return null without initializing a viewer", "[wlx][preview]")
{
    IMCodec::ImageLoader loader;
    REQUIRE(OIV::Win32::CreatePreviewBitmap(loader, nullptr, 64, 64) == nullptr);
    REQUIRE(OIV::Win32::CreatePreviewBitmap(loader, L"", 64, 64) == nullptr);
    TempImage file(L".bmp");
    WriteBmp(file);
    REQUIRE(OIV::Win32::CreatePreviewBitmap(loader, file.file.c_str(), 0, 64) == nullptr);
    REQUIRE(OIV::Win32::CreatePreviewBitmap(loader, file.file.c_str(), 64, -1) == nullptr);
    REQUIRE(OIV::Win32::CreatePreviewBitmap(loader, file.file.c_str(), INT_MAX, INT_MAX) == nullptr);
    REQUIRE(OIV::Win32::CreatePreviewBitmap(loader, (file.folder / L"missing.png").c_str(), 64, 64) == nullptr);
    constexpr char corrupt[] = "not an image";
    file.Write(corrupt, sizeof(corrupt));
    REQUIRE(OIV::Win32::CreatePreviewBitmap(loader, file.file.c_str(), 64, 64) == nullptr);
}

TEST_CASE("Concurrent preview orientation transforms own their workers", "[wlx][preview][concurrency]")
{
    std::latch start(2);
    const auto transform = [&](std::byte marker)
    {
        auto item                                = std::make_shared<IMCodec::ImageItem>();
        item->descriptor.width                   = 2048;
        item->descriptor.height                  = 1024;
        item->descriptor.rowPitchInBytes         = 2048 * 4;
        item->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_B8_G8_R8_A8;
        item->itemType                           = IMCodec::ImageItemType::Image;
        item->data.Allocate(2048 * 1024 * 4);
        std::fill_n(item->data.data(), item->data.size(), marker);
        auto image = std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown);
        start.count_down();
        start.wait();
        return IMUtil::ImageUtil::Transform({IMUtil::AxisAlignedRotation::Rotate90CW, IMUtil::AxisAlignedFlip::None},
                                            image);
    };
    auto first  = std::async(std::launch::async, transform, std::byte{0x11});
    auto second = std::async(std::launch::async, transform, std::byte{0x22});
    for (const auto& [image, marker] :
         {std::pair{first.get(), std::byte{0x11}}, std::pair{second.get(), std::byte{0x22}}})
    {
        REQUIRE(image->GetWidth() == 1024);
        REQUIRE(image->GetHeight() == 2048);
        REQUIRE(image->GetBuffer()[0] == marker);
        REQUIRE(image->GetBuffer()[image->GetSizeInMemory() - 1] == marker);
    }
}
#endif
