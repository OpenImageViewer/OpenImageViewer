#pragma once
#include <FreeTypeWrapper/FreeTypeConnector.h>
namespace FreeType
{

    class FreeTypeHelper
    {
    public:
        static IMCodec::ImageSharedPtr CreateRGBAText(const FreeTypeConnector::TextCreateParams& createParams)
        {
            FreeTypeConnector::Bitmap rasterizedText;
            FreeTypeConnector::GetSingleton().CreateBitmap(createParams, rasterizedText);

            if (rasterizedText.width != 0 && rasterizedText.height != 0)
                return BitmapToRGBAImage(rasterizedText);
            else
                return IMCodec::ImageSharedPtr();

        }

        static IMCodec::ImageSharedPtr BitmapToRGBAImage(const FreeTypeConnector::Bitmap& rasterizedText)
        {
            using namespace IMCodec;
            ImageDescriptor props;
            props.fProperties.NumSubImages = 0;
            props.fProperties.Height = rasterizedText.height;
            props.fData = rasterizedText.buffer.Clone();
            props.fProperties.RowPitchInBytes = rasterizedText.rowPitch;
            props.fProperties.Width = rasterizedText.width;
            props.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
            ImageSharedPtr textImage = ImageSharedPtr(new Image(props));
            return textImage;
        }
    };
}


    

