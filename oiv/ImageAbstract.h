#pragma once
#include "ImageProperties.h"
#include <string>
namespace OIV
{

    class Image
    {
    private:
        ImageProperies fProperies;

    public:
        Image(const ImageProperies& propeerties,double loadTime);
        virtual ~Image() { if (fProperies.ImageBuffer != NULL) delete[]fProperies.ImageBuffer;}
        bool Load(std::string filePath);
        void Unload();
        double GetLoadTime() const;

        // Query methods

        
        const void* GetBuffer() const { return fProperies.ImageBuffer; }
        size_t GetWidth() const { return fProperies.Width; }
        size_t GetHeight() const { return fProperies.Height; }
        size_t GetRowPitchInBytes() const { return fProperies.RowPitchInBytes; }
        size_t GetBitsPerTexel() const { return fProperies.BitsPerTexel; }
        ImageFormat GetFormat() const { return fProperies.Format; }
        
        size_t GetRowPitchInTexels() const { return GetRowPitchInBytes() / GetBytesPerTexel(); }
        size_t GetSlicePitchInBytes() const { return GetRowPitchInBytes() * GetHeight(); }
        size_t GetSlicePitchInTexels() const { return GetRowPitchInTexels() * GetHeight(); }
        size_t GetTotalPixels() const { return GetWidth() * GetHeight(); }
        size_t GetBytesPerTexel() const { return GetBitsPerTexel() / 8; }
        size_t GetNumberOfUniqueColors() const { throw std::exception("Not implemented"); }
    private:
         double fLoadTime;
    };

 
}
