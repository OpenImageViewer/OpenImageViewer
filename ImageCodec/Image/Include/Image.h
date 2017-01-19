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
        uint8_t* GetBuffer() const { return fProperies.ImageBuffer; }
        const uint8_t* GetConstBuffer() const { return fProperies.ImageBuffer; }

        std::size_t GetWidth() const { return fProperies.Width; }
        std::size_t GetHeight() const { return fProperies.Height; }
        std::size_t GetRowPitchInBytes() const { return fProperies.RowPitchInBytes; }
        std::size_t GetBitsPerTexel() const { return GetTexelFormatSize(fProperies.TexelFormatDecompressed); }
        std::size_t GetBytesPerRowOfPixels() const { return GetWidth() * GetBytesPerTexel(); }
        std::size_t GetRowPitchInTexels() const { return GetRowPitchInBytes() / GetBytesPerTexel(); }
        std::size_t GetSlicePitchInBytes() const { return GetRowPitchInBytes() * GetHeight(); }
        std::size_t GetSlicePitchInTexels() const { return GetRowPitchInTexels() * GetHeight(); }
        std::size_t GetTotalPixels() const { return GetWidth() * GetHeight(); }
        std::size_t GetTotalSizeOfImageTexels() const { return GetTotalPixels() *  GetBytesPerTexel(); }
        std::size_t GetBytesPerTexel() const { return GetBitsPerTexel() / 8; }
        std::size_t GetSizeInMemory() const { return GetRowPitchInBytes() * GetHeight(); }

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
