#pragma once
#include <OIVImage/OIVBaseImage.h>
#include <ImageUtil/AxisAlignedTransform.h>
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
     
        static OIVBaseImageSharedPtr ResampleImage(OIVBaseImageSharedPtr image, LLUtils::PointI32 scale);
    };
}
