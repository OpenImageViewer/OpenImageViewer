#pragma once
#include "OIVBaseImage.h"
namespace OIV
{
    struct RawBufferParams
    {
        const std::byte* buffer;
        uint32_t width;
        uint32_t height;
        uint32_t rowPitch;
        OIV_TexelFormat texelFormat;
    };


    class OIVRawImage : public OIVBaseImage
    {
    public:
        OIVRawImage(ImageSource customImagesource) : fImageSource(customImagesource) { }
        ResultCode Load(const RawBufferParams& loadParams);
    private:
        ImageSource fImageSource;

    };
}