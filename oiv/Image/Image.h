#pragma once
#include "ImageProperties.h"
#include <memory>
namespace OIV
{

    class Image
    {
    private:
        ImageProperies fProperies;

    public:
        Image(const ImageProperies& propeerties,double loadTime);
        virtual ~Image() { if (fProperies.ImageBuffer != nullptr) delete[]fProperies.ImageBuffer;}

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
        size_t GetBytesPerTexel() const { return GetBitsPerTexel() / 8; }
        size_t GetNumberOfUniqueColors() const { throw std::exception("Not implemented"); }
        ImageType GetImageType() const { return fProperies.Type; }
    private:
         double fLoadTime;
    };

    typedef std::shared_ptr<Image> ImageSharedPtr;
 
}
