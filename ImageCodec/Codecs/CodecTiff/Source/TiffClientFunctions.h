#pragma once
#include <cstdint>
#include <tiffio.h>
#include <tiffio.hxx>
#include <string>

namespace IMCodec
{

    struct tiff_bufferInfo
    {
        const uint8_t* buffer;
        uint32_t size;
        uint32_t currentPos;
    };


    tsize_t tiff_read(thandle_t handle, tdata_t data, tsize_t size)
    {
        tiff_bufferInfo* bufferInfo = reinterpret_cast<tiff_bufferInfo*>(handle);
        memcpy(data, bufferInfo->buffer + bufferInfo->currentPos, size);
        bufferInfo->currentPos += size;// bufferInfo->buffer +
        return size;
    }


    tsize_t tiff_write(thandle_t handle, tdata_t data, tsize_t size)
    {
        return  0;
    }
    toff_t tiff_seek(thandle_t handle, toff_t offset, int whence)
    {
        tiff_bufferInfo* bufferInfo = reinterpret_cast<tiff_bufferInfo*>(handle);
        uint32_t& currentPos = bufferInfo->currentPos;

        switch (whence)
        {
        case SEEK_SET:
            currentPos = offset;
            break;
        case SEEK_CUR:
            currentPos += offset;
            break;
        case SEEK_END:
            currentPos += bufferInfo->size + offset;
            break;
        default:
            int k = 0;
            break;
        }
        return bufferInfo->currentPos;
    }
    int tiff_close(thandle_t)
    {
        return  0;
    }
    toff_t tiff_size(thandle_t handle)
    {
        tiff_bufferInfo* bufferInfo = reinterpret_cast<tiff_bufferInfo*>(handle);
        return bufferInfo->size;
    }
    int tiff_map(thandle_t, tdata_t*, toff_t*)
    {
        return  0;
    }
    void tiff_unmap(thandle_t, tdata_t, toff_t)
    {

    }

    TIFF* ReadTiffFromMemory(tiff_bufferInfo& bufferInfo)
    {
        TIFF* tif = TIFFClientOpen("TifChannel", "r",
            reinterpret_cast<thandle_t>(&bufferInfo),
            tiff_read,
            tiff_write,
            tiff_seek,
            tiff_close,
            tiff_size,
            tiff_map,
            tiff_unmap);

        return tif;
    }
}