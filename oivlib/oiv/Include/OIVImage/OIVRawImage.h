#pragma once

#include "OIVBaseImage.h"
#include "../defs.h"
#include <ImageUtil/AxisAlignedTransform.h>

namespace OIV
{
    struct RawBufferParams
    {
        const std::byte* buffer;
        uint32_t width;
        uint32_t height;
        uint32_t rowPitch;
        IMCodec::TexelFormat texelFormat;
    };


    class OIVRawImage : public OIVBaseImage
    {
    public:
        OIVRawImage(ImageSource customImagesource) : OIVBaseImage(customImagesource) { }
        ResultCode Load(const RawBufferParams& loadParams, const IMUtil::AxisAlignedTransform& transform);
    };
}