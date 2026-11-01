#include "WlxPreview.h"
#include "ImageBitmap.h"

#include <ImageUtil/ImageUtil.h>
#include <LWS/Bitmap.hpp>
#include <ImageProcessing/FileImage.h>

#include <cstdint>
#include <cstring>
#include <limits>

namespace OIV::Win32
{
    namespace
    {
        // Containers (animations, pages, and icons) need a concrete image before format conversion. Walk in file
        // order, preferring the main image when it has pixels, as OIV does when the main item is displayable.
        IMCodec::ImageSharedPtr FirstDisplayableImage(const IMCodec::ImageSharedPtr& image)
        {
            IMCodec::ImageSharedPtr result;
            if (image != nullptr)
            {
                if (image->GetItemType() != IMCodec::ImageItemType::Container && image->GetWidth() != 0 &&
                    image->GetHeight() != 0 && image->GetBuffer() != nullptr)
                    result = image;
                for (uint32_t index = 0; result == nullptr && index < image->GetNumSubImages(); ++index)
                    result = FirstDisplayableImage(image->GetSubImage(static_cast<uint16_t>(index)));
            }
            return result;
        }
    }  // namespace

    HBITMAP CreatePreviewBitmap(IMCodec::ImageLoader& loader, const wchar_t* fileName, int width, int height) noexcept
    {
        constexpr uint64_t bytesPerPixel = 4;
        if (fileName == nullptr || *fileName == L'\0' || width <= 0 || height <= 0 ||
            static_cast<uint64_t>(width) * height * bytesPerPixel > std::numeric_limits<DWORD>::max())
            return nullptr;

        HBITMAP result = nullptr;
        try
        {
            IMCodec::Parameters parameters;
            parameters.SetCustom(LLUTILS_TEXT("canvasWidth"), width);
            parameters.SetCustom(LLUTILS_TEXT("canvasHeight"), height);
            IMCodec::ItemMetaDataSharedPtr metadata;
            auto image = FirstDisplayableImage(DecodeFileImage(
                loader, fileName, metadata,
                IMCodec::PluginTraverseMode::AnyPlugin | IMCodec::PluginTraverseMode::AnyFileType,
                IMCodec::ImageLoadFlags::Preview | IMCodec::ImageLoadFlags::LoadOnlyFirstImage, parameters));
            if (image == nullptr)
                return nullptr;
            // Host previews bypass ImageState but need the same transfer and display encoding.
            image = IMUtil::ImageUtil::EncodeLinearImageForDisplay(IMUtil::ImageUtil::ApplyTransferFunction(image));
            image = IMUtil::ImageUtil::ConvertImageWithNormalization(image, IMCodec::TexelFormat::I_B8_G8_R8_A8, false);
            if (image == nullptr)
                return nullptr;

            // Use the same premultiplied CPU resizer as the sidebar. Compositing onto white also gives hosts that
            // copy the HBITMAP with ordinary GDI a defined result for transparent pixels.
            const LWS::Bitmap source(MakeBgraBitmapView(*image));
            const auto thumbnail = source.resize(width, height, {255, 255, 255, 255});
            const auto pixels    = thumbnail->GetBuffer();
            BITMAPINFO info{};
            info.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth       = width;
            info.bmiHeader.biHeight      = -height;
            info.bmiHeader.biPlanes      = 1;
            info.bmiHeader.biBitCount    = 32;
            info.bmiHeader.biCompression = BI_RGB;
            info.bmiHeader.biSizeImage   = static_cast<DWORD>(pixels.pixels.size());
            void* destination            = nullptr;
            result                       = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &destination, nullptr, 0);
            if (result != nullptr)
                std::memcpy(destination, pixels.pixels.data(), pixels.pixels.size());
        }
        catch (...)
        {
            // Preview failure must remain local to this optional host request.
            result = nullptr;
        }
        return result;
    }
}  // namespace OIV::Win32
