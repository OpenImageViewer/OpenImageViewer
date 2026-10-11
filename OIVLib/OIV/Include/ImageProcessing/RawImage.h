#pragma once
#include <Image.h>
#include <ImageUtil/AxisAlignedTransform.h>

namespace OIV
{
    // Borrowed raw input is copied before returning, preserving row padding and format.
    struct RawBufferParams
    {
        // Input bytes, readable for rowPitch multiplied by height during creation.
        const std::byte* buffer;
        // Logical image width in texels.
        uint32_t width;
        // Logical image height in rows.
        uint32_t height;
        // Input row stride in bytes, including padding.
        uint32_t rowPitch;
        // Storage and decompressed format of the input bytes.
        IMCodec::TexelFormat texelFormat;
    };
    IMCodec::ImageSharedPtr CreateRawImage(const RawBufferParams& loadParams,
                                           const IMUtil::AxisAlignedTransform& transform);
}  // namespace OIV
