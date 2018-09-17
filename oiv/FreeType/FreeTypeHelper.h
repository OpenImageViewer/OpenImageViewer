#pragma once
#if OIV_BUILD_FREETYPE == 1
#include "FreeTypeConnector.h"
namespace OIV
{

    class FreeTypeHelper
    {
    public:
        static IMCodec::ImageSharedPtr CreateRGBAText(const std::string& text
            , const std::string& fontPath
            , uint16_t fontSize
            , LLUtils::Color backGroundColor
        )
        {
            FreeTypeConnector::Bitmap rasterizedText;
            FreeTypeConnector::GetSingleton().CreateBitmap(text, fontPath, fontSize, backGroundColor, rasterizedText);

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
            const size_t bufferSize = rasterizedText.rowPitch * rasterizedText.height;
            props.fData.Allocate(bufferSize);
            props.fData.Write(rasterizedText.buffer, 0, bufferSize);
            props.fProperties.RowPitchInBytes = rasterizedText.rowPitch;
            props.fProperties.Width = rasterizedText.width;
            props.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
            ImageSharedPtr textImage = ImageSharedPtr(new Image(props));
            return textImage;
        }
    };
}
#endif


    

