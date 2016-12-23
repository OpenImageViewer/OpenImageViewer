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
        if (GetBitsPerTexel() % 8 != 0)
            throw std::logic_error("Can not normalize a non byte aligned pixel format");
        
        size_t targetRowPitch = GetBytesPerRowOfPixels();

        if (GetRowPitchInBytes() != targetRowPitch)
        {
            uint8_t* newBuffer = new uint8_t[GetTotalSizeOfImageTexels()];
            for (size_t y = 0; y < GetHeight()  ;y++ )
                for (size_t x = 0; x < targetRowPitch; x++)
                {
                    size_t srcIndex = y * GetRowPitchInBytes() + x;
                    size_t dstIndex = y * targetRowPitch + x;
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

    void Image::Transform(AxisAlignedRTransform transform)
    {
        if (GetIsByteAligned() == false)
            throw std::logic_error("OIV::Image::Transom works only with byte aligned image formats");
        
        if (transform != AAT_None)
        {
            const uint8_t* src = fProperies.ImageBuffer;
            const size_t srcRowPitch = GetRowPitchInBytes();
            const size_t bytesPerRowOfPixels = GetBytesPerRowOfPixels();
            const size_t destRowPitch = bytesPerRowOfPixels;
            const size_t bytesPerTexel = GetBytesPerTexel();

            uint8_t* dest = new uint8_t[GetTotalSizeOfImageTexels()];

            for (size_t y = 0; y < fProperies.Height; y++)
                for (size_t x = 0; x < fProperies.Width; x++)
                {
                    const uint8_t* srcRow = src + y * srcRowPitch;
                    int idxDest;
                    
                    switch (transform)
                    {
                    case AAT_Rotate180:
                        idxDest = -x + fProperies.Width - 1 + (-y + fProperies.Height - 1) * fProperies.Width;
                        break;
                    case AAT_Rotate90CW:
                        idxDest = (fProperies.Height - 1 - y) + x * fProperies.Height;
                        break;
                    case AAT_Rotate90CCW:
                        idxDest = y + (fProperies.Width - 1 - x) * fProperies.Height;
                        break;
                    case AAT_FlipVertical:
                        idxDest = x + (-y + fProperies.Height - 1) * fProperies.Width;
                        break;
                    case AAT_FlipHorizontal:
                        idxDest = (fProperies.Width - 1 - x) + y * fProperies.Width;
                        break;

                    default:
                        throw std::runtime_error("Wrong or corrupted value");
                    }


                    
                    switch (bytesPerTexel)
                    {
                    case 1:
                        
                        PixelUtil::CopyTexel <PixelUtil::BitTexel8>(dest, idxDest, srcRow, x);
                        break;
                    case 2:
                        PixelUtil::CopyTexel<PixelUtil::BitTexel16>(dest, idxDest, srcRow, x);
                        break;
                    case 3:
                        PixelUtil::CopyTexel<PixelUtil::BitTexel24>(dest, idxDest, srcRow, x);
                        break;
                    case 4:
                        PixelUtil::CopyTexel<PixelUtil::BitTexel32>(dest, idxDest, srcRow, x);
                        break;
                    default:
                        throw std::exception("Wrong or corrupted value");
                    }
                }

            if (transform == AAT_Rotate90CW || transform == AAT_Rotate90CCW)
                std::swap(fProperies.Height, fProperies.Width);

            fProperies.RowPitchInBytes = fProperies.Width * bytesPerTexel;

            delete[]fProperies.ImageBuffer;
            fProperies.ImageBuffer = dest;
        }
    }
}
