#pragma once
#include "ImageProperties.h"
#include <memory>
#include "AxisAlignedTransform.h"

namespace OIV
{
    class Image
    {
    private:
        ImageProperies fProperies;

    public:
        Image(const ImageProperies& propeerties,double loadTime);
        virtual ~Image() { if (fProperies.ImageBuffer != nullptr) delete[]fProperies.ImageBuffer;}

        void Transform(AxisAlignedRTransform transform);

        

        // Query methods
        double GetLoadTime() const;
        const void* GetBuffer() const { return fProperies.ImageBuffer; }
        size_t GetWidth() const { return fProperies.Width; }
        size_t GetHeight() const { return fProperies.Height; }
        size_t GetRowPitchInBytes() const { return fProperies.RowPitchInBytes; }
        size_t GetBitsPerTexel() const { return fProperies.BitsPerTexel; }
        
        size_t GetRowPitchInTexels() const { return GetRowPitchInBytes() / GetBytesPerTexel(); }
        size_t GetSlicePitchInBytes() const { return GetRowPitchInBytes() * GetHeight(); }
        size_t GetSlicePitchInTexels() const { return GetRowPitchInTexels() * GetHeight(); }
        size_t GetTotalPixels() const { return GetWidth() * GetHeight(); }
        size_t GetTotalSizeOfImageTexels() const { return GetTotalPixels() *  GetBytesPerTexel(); } 
        size_t GetBytesPerTexel() const { return GetBitsPerTexel() / 8; }
        size_t GetSizeInMemory() const { return GetRowPitchInBytes() * GetHeight(); }
        size_t GetNumberOfUniqueColors() const { throw std::exception("Not implemented"); }
        ImageType GetImageType() const { return fProperies.Type; }
        
        template <class BIT_TEXEL_TYPE>
        __forceinline  void Image::CopyTexel(void* dest, const size_t idxDest, const void* src, const size_t idxSrc) const
        {
            reinterpret_cast<BIT_TEXEL_TYPE*>(dest)[idxDest] = reinterpret_cast<const  BIT_TEXEL_TYPE*>(src)[idxSrc];
        }

    private:
         double fLoadTime;
    };

    typedef std::shared_ptr<Image> ImageSharedPtr;
 
}
