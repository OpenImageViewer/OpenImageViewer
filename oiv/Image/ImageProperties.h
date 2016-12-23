#pragma once
#include <cstdint>

namespace OIV
{
    enum ImageType
    {
          IT_UNKNOWN
        , IT_BYTE_RGBA
        , IT_BYTE_BGR
        , IT_BYTE_BGRA
        , IT_BYTE_RGB
        , IT_BYTE_ARGB
        , IT_BYTE_ABGR
        , IT_FLOAT
        , IT_BYTE_8BIT
    };

    
    class ImageProperies
    {
    public:
        ImageProperies();

        bool IsInitialized() const;

        size_t Width;
        size_t Height;
        size_t RowPitchInBytes;
        size_t BitsPerTexel;
        ImageType Type;
        size_t NumSubImages;
        uint8_t* ImageBuffer;
    };
}
