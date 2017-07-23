#pragma once
#include <cstdint>
#include "TiffClientFunctions.h"

namespace IMCodec
{
    class TiffFile
    {
        tiff_bufferInfo fBufferInfo;
        TIFF* fTiff = nullptr;
    public:
        TIFF* GetTiff()
        {
            return fTiff;
        }
        TiffFile(const uint8_t* buffer, uint32_t size)
        {
            fBufferInfo = { buffer,size };
            fTiff = ReadTiffFromMemory(fBufferInfo);
        }
        ~TiffFile()
        {
            if (fTiff)
                TIFFClose(fTiff);
        }
    };
}
