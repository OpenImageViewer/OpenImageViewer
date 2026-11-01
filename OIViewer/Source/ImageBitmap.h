#pragma once
#include <Image.h>
#include <LWS/Bitmap.hpp>

namespace OIV
{
    // The caller retains an image already normalized to BGRA8 for as long as this borrowed view is used.
    // This adapter allocates and converts nothing. Codec and windowing dependencies meet only in the client.
    inline LWS::BitmapBuffer MakeBgraBitmapView(const IMCodec::Image& image)
    {
        return {
            .pixels   = {image.GetBuffer(), image.GetImageItem()->data.size()},
            .format   = LWS::BitmapPixelFormat::Bgra8,
            .rowOrder = LWS::BitmapRowOrder::TopDown,
            .width    = image.GetWidth(),
            .height   = image.GetHeight(),
            .rowPitch = image.GetRowPitchInBytes(),
        };
    }
}  // namespace OIV
