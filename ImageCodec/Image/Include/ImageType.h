#pragma once
#include <cstdint>
#include <stdexcept>

namespace IMCodec
{
    enum ImageType : uint16_t
    {
          IT_UNKNOWN
        , IT_BYTE_RGBA
        , IT_BYTE_BGR
        , IT_BYTE_BGRA
        , IT_BYTE_RGB
        , IT_BYTE_ARGB
        , IT_BYTE_ABGR
        , IT_FLOAT16
        , IT_FLOAT32
        , IT_FLOAT64
        , IT_BYTE_8BIT
    };

    uint8_t ImageTypeSize(ImageType imageType);

}
