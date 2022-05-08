#pragma once
#include "../OIVCommands.h"
#include <OIVImage/OIVBaseImage.h>
#include "../../oiv/Source/ImageUtil.h"
#include "../../oiv/Source/oiv.h"
#include "../../oiv/Source/ApiGlobal.h"
namespace OIV
{
    //Create an RGBA render compatible image, 
    // TODO: accept any render compatible image format.

    class OIVImageHelper
    {
    public:
        //TODO: add convertion parameters in case normalization is needed instead of the rainbow parameter.
        static OIVBaseImageSharedPtr ConvertImage(OIVBaseImageSharedPtr image, IMCodec::TexelFormat texelFormat, bool useRainbow)
        {
            if (image->GetImage()->GetTexelFormat() != texelFormat)
            {
                auto converted =  IMUtil::ImageUtil::ConvertImageWithNormalization(image->GetImage(), texelFormat, useRainbow);
                
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

        static OIVBaseImageSharedPtr GetRendererCompatibleImage(OIVBaseImageSharedPtr image, bool useRainbow)
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
        static OIVBaseImageSharedPtr ResampleImage(OIVBaseImageSharedPtr image, LLUtils::PointI32 scale)
        {
            auto resampled = ApiGlobal::sPictureRenderer->Resample(image->GetImage(), scale);
            if (resampled != nullptr)
            {
                return std::make_shared<OIVBaseImage>(ImageSource::GeneratedByLib, resampled);
            }
            else
            {
                return nullptr;
            }
        }
    };
}