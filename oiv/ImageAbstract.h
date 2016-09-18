#pragma once

//#define FREEIMAGE_LITTLEENDIAN
#include "OgreCommon.h"
#include "libs/FreeImage.h"
namespace OIV
{
    enum ImageType
    {
        IT_UNKNOWN
        ,IT_BITMAP
        ,IT_FLOAT
    };

    class Image
    {

    public:
        //Base abstract methods
        virtual size_t GetWidth() = 0;
        virtual size_t GetHeight() = 0;
        virtual size_t GetRowPitchInBytes() = 0;
        virtual size_t GetBitsPerTexel() = 0;
        virtual unsigned char* GetBuffer() = 0;
        virtual ImageType GetImageType() = 0;

        // Query methods
        size_t GetRowPitchInTexels() { return GetRowPitchInBytes() / GetBytesPerTexel(); }
        size_t GetSlicePitchInBytes() { return GetRowPitchInBytes() * GetHeight(); }
        size_t GetSlicePitchInTexels() { return GetRowPitchInTexels() * GetHeight(); }
        size_t GetTotalPixels() { return GetWidth() * GetHeight(); }
        unsigned short GetBytesPerTexel() { return GetBitsPerTexel() / 8; }
        bool NeedConvertionToBYTERGBA() { return GetImageType() != IT_BITMAP || GetBitsPerTexel() != 32; }


        size_t GetNumberOfSubImages() { throw std::exception("Not implemented"); }
        size_t GetNumberOfUniqueColors() { throw std::exception("Not implemented"); }



        virtual Image* ConverToRGBA() = 0;
        virtual void Unload() {}
        virtual bool Load(std::string filePath) = 0;
        virtual bool IsOpened() = 0;

        virtual ~Image() {}
    };
}
