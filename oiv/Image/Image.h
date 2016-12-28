#pragma once
#include <memory>
#include <stdexcept>
#include "AxisAlignedTransform.h"
#include "ImageProperties.h"

namespace OIV
{
    class Image
    {
    private:
        ImageProperies fProperies;

    public:
        Image(const ImageProperies& propeerties,double loadTime);
        void Normalize();
        virtual ~Image() { if (fProperies.ImageBuffer != nullptr) delete[]fProperies.ImageBuffer;}

        void Transform(AxisAlignedRTransform transform);

        
        //Internal methods
        const ImageProperies& GetProperties() const { return fProperies; } 


        // Query methods
        double GetLoadTime() const;
        const void* GetBuffer() const { return fProperies.ImageBuffer; }

        std::size_t GetWidth() const { return fProperies.Width; }
        std::size_t GetHeight() const { return fProperies.Height; }
        std::size_t GetRowPitchInBytes() const { return fProperies.RowPitchInBytes; }
        std::size_t GetBitsPerTexel() const { return fProperies.BitsPerTexel; }
        std::size_t GetBytesPerRowOfPixels() const { return GetWidth() * GetBytesPerTexel(); }
        std::size_t GetRowPitchInTexels() const { return GetRowPitchInBytes() / GetBytesPerTexel(); }
        std::size_t GetSlicePitchInBytes() const { return GetRowPitchInBytes() * GetHeight(); }
        std::size_t GetSlicePitchInTexels() const { return GetRowPitchInTexels() * GetHeight(); }
        std::size_t GetTotalPixels() const { return GetWidth() * GetHeight(); }
        std::size_t GetTotalSizeOfImageTexels() const { return GetTotalPixels() *  GetBytesPerTexel(); } 
        std::size_t GetBytesPerTexel() const { return GetBitsPerTexel() / 8; }
        std::size_t GetSizeInMemory() const { return GetRowPitchInBytes() * GetHeight(); }
        std::size_t GetNumberOfUniqueColors() const { throw std::runtime_error("Not implemented"); }

        bool GetIsRowPitchNormalized() const {return GetRowPitchInBytes() == GetBytesPerRowOfPixels();}
        bool GetIsByteAligned() const { return GetBitsPerTexel() % 8 == 0; }

        ImageType GetImageType() const { return fProperies.Type; }
        
    private:
         double fLoadTime;
    };

    typedef std::shared_ptr<Image> ImageSharedPtr;
 
}
