#pragma once
#include <FreeTypeWrapper/FreeTypeConnector.h>
namespace FreeType
{

    class FreeTypeHelper
    {
    public:
        static IMCodec::ImageSharedPtr CreateRGBAText(const TextCreateParams& createParams, TextMetrics* metrics)
        {
            FreeTypeConnector::Bitmap rasterizedText;
            FreeTypeConnector::GetSingleton().CreateBitmap(createParams, rasterizedText, metrics);

            if (rasterizedText.width != 0 && rasterizedText.height != 0)
                return BitmapToRGBAImage(rasterizedText);
            else
                return IMCodec::ImageSharedPtr();

        }

        static IMCodec::ImageSharedPtr BitmapToRGBAImage(const FreeTypeConnector::Bitmap& rasterizedText)
        {
            using namespace IMCodec;
            ImageItemSharedPtr imageItem = std::make_shared<ImageItem>();
            ImageDescriptor& props = imageItem->descriptor;
            imageItem->itemType = ImageItemType::Image;
            props.height= rasterizedText.height;
            props.width = rasterizedText.width;
            props.rowPitchInBytes = rasterizedText.rowPitch;
            props.texelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;

            imageItem->data = rasterizedText.buffer.Clone();
            ImageSharedPtr textImage = std::make_shared<Image>(imageItem, ImageItemType::Unknown);

            return textImage;
        }
    };
}


    

