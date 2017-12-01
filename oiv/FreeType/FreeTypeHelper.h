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
            ImageProperies props;
            props.NumSubImages = 0;
            props.Height = rasterizedText.height;
            props.ImageBuffer = rasterizedText.buffer;
            props.RowPitchInBytes = rasterizedText.rowPitch;
            props.Width = rasterizedText.width;
            props.TexelFormatDecompressed = TexelFormat::I_R8_G8_B8_A8;
            ImageSharedPtr textImage = ImageSharedPtr(new Image(props, ImageData()));
            return textImage;
        }
    };
}
#endif


    

