#include "ImageType.h"
namespace IMCodec
{
    uint8_t ImageTypeSize(ImageType imageType)
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
            throw std::runtime_error("Wrong or corrupted value");
        }
    }
}