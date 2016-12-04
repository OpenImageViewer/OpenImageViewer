#include <Image/Image.h>
#include <chrono>
#include <stdint.h>
namespace OIV
{
    struct alignas(1) BitTexel8                         { uint8_t X; };
                                                                     
    struct alignas(1) BitTexel16 : public BitTexel8     { uint8_t Y; };
                                                                     
    struct alignas(1) BitTexel24 : public BitTexel16    { uint8_t Z; };
                                                                     
    struct alignas(1) BitTexel32 : public BitTexel24    { uint8_t W; };

    Image::Image(const ImageProperies& propeerties, double loadTime)
    {
        fProperies = propeerties;
        fLoadTime = loadTime;
    }

    double Image::GetLoadTime() const
    {
        return fLoadTime;
    }

    

    void Image::Transform(AxisAlignedRTransform transform)
    {
        if (GetBitsPerTexel() % 8 != 0)
            throw std::exception("Axis aligned transformation is allowed only on byte aligned texel size");
        
        if (transform != AAT_None)
        {
            void* src = fProperies.ImageBuffer;
            void* dest = new char*[GetTotalSizeOfImageTexels()];
            const size_t bytePerTexels = GetBytesPerTexel();

            for (size_t x = 0; x < fProperies.Width; x++)
                for (size_t y = 0; y < fProperies.Height; y++)
                {
                    int idxSrc = x + y * fProperies.Width;
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
                        throw std::exception("Wrong or corrupted value");
                    }


                    switch (bytePerTexels)
                    {
                    case 1:
                        CopyTexel <BitTexel8>(dest, idxDest, src, idxSrc);
                        break;
                    case 2:
                        CopyTexel<BitTexel16>(dest, idxDest, src, idxSrc);
                        break;
                    case 3:
                        CopyTexel<BitTexel24>(dest, idxDest, src, idxSrc);
                        break;
                    case 4:
                        CopyTexel<BitTexel32>(dest, idxDest, src, idxSrc);
                        break;
                    default:
                        throw std::exception("Wrong or corrupted value");
                    }
                }

            if (transform == AAT_Rotate90CW || transform == AAT_Rotate90CCW)
                std::swap(fProperies.Height, fProperies.Width);

            fProperies.RowPitchInBytes = fProperies.Width * (fProperies.BitsPerTexel / 8);

            delete[]fProperies.ImageBuffer;
            fProperies.ImageBuffer = dest;
        }
    }
}