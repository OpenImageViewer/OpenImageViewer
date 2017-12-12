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
            return BitmapToRGBAImage(rasterizedText);
        }

        static IMCodec::ImageSharedPtr BitmapToRGBAImage(const FreeTypeConnector::Bitmap& rasterizedText)
        {
            using namespace IMCodec;
            ImageDescriptor props;
            props.fProperties.NumSubImages = 0;
            props.fProperties.Height = rasterizedText.height;
            props.fData.AllocateAndWrite(rasterizedText.buffer, rasterizedText.rowPitch * rasterizedText.height);
            props.fProperties.RowPitchInBytes = rasterizedText.rowPitch;
            props.fProperties.Width = rasterizedText.width;
            props.fProperties.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
            ImageSharedPtr textImage = ImageSharedPtr(new Image(props));
            return textImage;
        }
    };
}
#endif


    

