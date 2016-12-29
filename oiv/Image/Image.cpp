#include <chrono>
#include <Image/Image.h>
#include <Image/PixelUtil.h>
#include "ImageUtil.h"

namespace OIV
{
    Image::Image(const ImageProperies& propeerties, double loadTime)
    {
        fProperies = propeerties;
        fLoadTime = loadTime;
    }
    
    void Image::Normalize()
    {
        if (GetIsByteAligned() == false)
            throw std::logic_error("Can not normalize a non byte aligned pixel format");

        if (GetIsRowPitchNormalized() == false)
        {
            std::size_t targetRowPitch = GetBytesPerRowOfPixels();
            uint8_t* newBuffer = new uint8_t[GetTotalSizeOfImageTexels()];
            for (std::size_t y = 0; y < GetHeight()  ;y++ )
                for (std::size_t x = 0; x < targetRowPitch; x++)
                {
                    std::size_t srcIndex = y * GetRowPitchInBytes() + x;
                    std::size_t dstIndex = y * targetRowPitch + x;
                    newBuffer[dstIndex] = fProperies.ImageBuffer[srcIndex];

                }
                
            delete[]fProperies.ImageBuffer;

            fProperies.ImageBuffer = newBuffer;
            fProperies.RowPitchInBytes = targetRowPitch;
        }
        // else already normalized.
    }



    double Image::GetLoadTime() const
    {
        return fLoadTime;
    }

    void Image::Transform(OIV_AxisAlignedRTransform transform)
    {
        if (GetIsByteAligned() == false)
            throw std::logic_error("OIV::Image::Transom works only with byte aligned image formats");
        
        if (transform != AAT_None)
        {
            
            uint8_t* dest = new uint8_t[GetTotalSizeOfImageTexels()];
            PixelUtil::TransformTexelsInfo desc;
            desc.transform = transform;
            desc.dstBuffer = dest;
            desc.srcBuffer = fProperies.ImageBuffer;
            desc.width = GetWidth();
            desc.height = GetHeight();
            desc.bytesPerTexel = GetBytesPerTexel();
            desc.srcRowPitch = GetRowPitchInBytes();
            desc.endRow = 0;
            desc.startCol = 0;
            desc.endCol = GetWidth();
            desc.startRow = 0;
            desc.endRow = GetHeight();

            PixelUtil::TransformTexels(desc);

            if (transform == AAT_Rotate90CW || transform == AAT_Rotate90CCW)
                std::swap(fProperies.Height, fProperies.Width);

            fProperies.RowPitchInBytes = fProperies.Width * GetBytesPerTexel();

            delete[]fProperies.ImageBuffer;
            fProperies.ImageBuffer = dest;
        }
    }
}
