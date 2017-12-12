#pragma once
#include <memory> // for shared_ptr
#include "ImageProperties.h"

namespace IMCodec
{
    class Image final
    {

    public:
        Image(const ImageDescriptor& properties)
            : fDescriptor(properties)

        {

        }
        
        //Image(const Image& rhs) = delete;
        Image operator= (const Image& rhs) = delete;



        //Internal methods
        const ImageDescriptor& GetDescriptor() const { return fDescriptor; }

        // Query methods
        uint8_t* GetBufferAt(int32_t x, int32_t y) const { return &fDescriptor.fData.GetBuffer()[y * GetRowPitchInBytes() + x * GetBytesPerTexel()]; }
        uint8_t* GetBuffer() const { return fDescriptor.fData.GetBuffer(); }
        const uint8_t* GetConstBuffer() const { return fDescriptor.fData.GetBuffer(); }
        uint32_t GetNumSubImages() const {return fDescriptor.fProperties.NumSubImages;}
        uint32_t GetWidth() const { return fDescriptor.fProperties.Width; }
        uint32_t GetHeight() const { return fDescriptor.fProperties.Height; }
        uint32_t GetRowPitchInBytes() const { return fDescriptor.fProperties.RowPitchInBytes; }
        uint32_t GetBitsPerTexel() const { return GetTexelFormatSize(fDescriptor.fProperties.TexelFormatDecompressed); }
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
        
        TexelFormat GetImageType() const { return fDescriptor.fProperties.TexelFormatDecompressed; }
        TexelFormat GetOriginalTexelFormat() const { return fDescriptor.fProperties.TexelFormatStorage; }

    private:
        const ImageDescriptor fDescriptor;
    };

    typedef std::shared_ptr<Image> ImageSharedPtr;
 
}
