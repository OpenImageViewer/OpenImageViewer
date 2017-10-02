#pragma once
#include <memory> // for shared_ptr
#include "ImageProperties.h"

namespace IMCodec
{
    class Image
    {

    public:
        Image(const ImageProperies& properties, const ImageData& imageData);
        virtual ~Image()
        {
            if (fProperies.ImageBuffer != nullptr)
            {
                delete[]fProperies.ImageBuffer;
                const_cast<ImageProperies&>(fProperies).ImageBuffer = nullptr;
            }
        }
        //Image(const Image& rhs) = delete;
        Image operator= (const Image& rhs) = delete;



        //Internal methods
        const ImageProperies& GetProperties() const { return fProperies; }
        const ImageData& GetData() const { return fImageData; }

        // Query methods
        uint8_t* GetBufferAt(int32_t x, int32_t y) const { return &fProperies.ImageBuffer[y * GetRowPitchInBytes() + x * GetBytesPerTexel()]; }
        uint8_t* GetBuffer() const { return fProperies.ImageBuffer; }
        const uint8_t* GetConstBuffer() const { return fProperies.ImageBuffer; }

        uint32_t GetWidth() const { return fProperies.Width; }
        uint32_t GetHeight() const { return fProperies.Height; }
        uint32_t GetRowPitchInBytes() const { return fProperies.RowPitchInBytes; }
        uint32_t GetBitsPerTexel() const { return GetTexelFormatSize(fProperies.TexelFormatDecompressed); }
        uint32_t GetBytesPerRowOfPixels() const { return GetWidth() * GetBytesPerTexel(); }
        uint32_t GetRowPitchInTexels() const { return GetRowPitchInBytes() / GetBytesPerTexel(); }
        uint32_t GetSlicePitchInBytes() const { return GetRowPitchInBytes() * GetHeight(); }
        uint32_t GetSlicePitchInTexels() const { return GetRowPitchInTexels() * GetHeight(); }
        uint32_t GetTotalPixels() const { return GetWidth() * GetHeight(); }
        uint32_t GetTotalSizeOfImageTexels() const { return GetTotalPixels() *  GetBytesPerTexel(); }
        uint32_t GetBytesPerTexel() const { return GetBitsPerTexel() / 8; }
        uint32_t GetSizeInMemory() const { return GetRowPitchInBytes() * GetHeight(); }

        bool GetIsRowPitchNormalized() const { return GetRowPitchInBytes() == GetBytesPerRowOfPixels(); }
        bool GetIsByteAligned() const { return GetBitsPerTexel() % 8 == 0; }
        
        TexelFormat GetImageType() const { return fProperies.TexelFormatDecompressed; }
        TexelFormat GetOriginalTexelFormat() const { return fProperies.TexelFormatStorage; }

    private:
        const ImageData fImageData;
        const ImageProperies fProperies;
    };

    typedef std::shared_ptr<Image> ImageSharedPtr;
 
}
