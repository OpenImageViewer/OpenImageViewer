#pragma once
#include "../OIVCommands.h"
#include <OIVImage/OIVBaseImage.h>
#include <ImageUtil/AxisAlignedTransform.h>
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
        static OIVBaseImageSharedPtr ConvertImage(OIVBaseImageSharedPtr image, IMCodec::TexelFormat texelFormat, bool useRainbow);

        static OIVBaseImageSharedPtr GetRendererCompatibleImage(OIVBaseImageSharedPtr image, bool useRainbow);
     
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