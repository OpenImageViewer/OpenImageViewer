#include "OIVImageHelper.h"
#include <ImageUtil/ImageUtil.h>

namespace OIV
{
    OIVBaseImageSharedPtr OIVImageHelper::ConvertImage(OIVBaseImageSharedPtr image, IMCodec::TexelFormat texelFormat, bool useRainbow)
    {
        if (image->GetImage()->GetTexelFormat() != texelFormat)
        {
            auto converted = IMUtil::ImageUtil::ConvertImageWithNormalization(image->GetImage(), texelFormat, useRainbow);

            if (converted != nullptr)
            {
                OIVBaseImageSharedPtr convertedImage = std::make_unique<OIVBaseImage>(ImageSource::GeneratedByLib, converted);
                return convertedImage;
            }
            else
            {
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Unable to convert image");
            }

        }
        else
        {
            return image;
        }
    }

    OIVBaseImageSharedPtr OIVImageHelper::GetRendererCompatibleImage(OIVBaseImageSharedPtr image, bool useRainbow)
    {
        if (image->GetImage()->GetTexelFormat() != IMCodec::TexelFormat::I_R8_G8_B8_A8)
        {
            return ConvertImage(image, IMCodec::TexelFormat::I_R8_G8_B8_A8, useRainbow);
        }
        else
        {
            return image;
        }
    }
}