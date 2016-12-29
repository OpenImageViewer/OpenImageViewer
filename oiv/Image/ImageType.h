#pragma once
#include <cstdint>
#include <stdexcept>

namespace OIV
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

    
    __forceinline  uint8_t ImageTypeSize(ImageType imageType)
    {
        switch (imageType)
        {
        case IT_BYTE_8BIT:
            return 8;
         case IT_FLOAT16:
             return 16;
        case IT_BYTE_BGR:
        case IT_BYTE_RGB:
            return 24;
        case IT_BYTE_RGBA:
        case IT_BYTE_BGRA:
        case IT_BYTE_ARGB:
        case IT_BYTE_ABGR:
        case IT_FLOAT32:
            return 32;
        case IT_FLOAT64:
            return 64;
        default:
            throw std::runtime_error("Wrong or correupted value");
        }
    }
}
