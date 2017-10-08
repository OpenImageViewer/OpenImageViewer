#pragma once
#include "FreeTypeConnector.h"
#include "ImageUtil.h"

namespace OIV
{
    class FreeTypeHelper
    {
    public:
        static IMCodec::ImageSharedPtr CreateRGBAText(const std::vector<std::string>& text, const std::string& fontPath, uint16_t fontSize)
        {
            FreeTypeConnector::Bitmap rasterizedText;
            //std::string fontName = "C:\\Windows\\Fonts\\Arial.ttf";
            //std::string fontName = "C:\\Windows\\Fonts\\consola.ttf";
            /*std::vector<std::string> text;
            text.push_back("HELLO");
            text.push_back("WORLD");*/
            FreeTypeConnector::GetSingleton().CreateBitmap(text, fontPath,fontSize, rasterizedText);
            return BitmapToRGBAImage(rasterizedText);
            
        }

        static IMCodec::ImageSharedPtr BitmapToRGBAImage(const FreeTypeConnector::Bitmap& rasterizedText)
        {
            using namespace IMCodec;
            ImageProperies props;
            props.NumSubImages = 0;
            props.Height = rasterizedText.height;
            props.ImageBuffer = rasterizedText.buffer;
            props.RowPitchInBytes = rasterizedText.width;
            props.Width = rasterizedText.width;
            props.TexelFormatDecompressed = TexelFormat::TF_I_A8;
            ImageSharedPtr textImage = ImageSharedPtr(new Image(props, ImageData()));
            textImage = IMUtil::ImageUtil::Convert(textImage, TexelFormat::TF_I_R8_G8_B8_A8);
            return textImage;
        }
    };
}





