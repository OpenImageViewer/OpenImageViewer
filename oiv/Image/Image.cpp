#include <algorithm>
#include <chrono>
#include <thread>
#include "Image.h"
#include "PixelUtil.h"

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
        using namespace std;
        if (GetIsByteAligned() == false)
            throw std::logic_error("OIV::Image::Transom works only with byte aligned image formats");
        
        if (transform != AAT_None)
        {
            uint8_t* dest = new uint8_t[GetTotalSizeOfImageTexels()];

            const size_t MegaBytesPerThread = 6;
            static const uint8_t MaxGlobalThrads = 32;
            static uint8_t maxThreads = static_cast<uint8_t>(
                min(static_cast<unsigned int>(MaxGlobalThrads), max(1u, thread::hardware_concurrency() - 1)));
            static thread threads[MaxGlobalThrads];


            const size_t bytesPerThread = MegaBytesPerThread * 1024 * 1024;
            const uint8_t totalThreads = std::min(maxThreads, static_cast<uint8_t>(GetTotalSizeOfImageTexels() /  bytesPerThread));
            PixelUtil::TransformTexelsInfo descTemplate;
            descTemplate.transform = transform;
            descTemplate.dstBuffer = dest;
            descTemplate.srcBuffer = fProperies.ImageBuffer;
            descTemplate.width = GetWidth();
            descTemplate.height = GetHeight();
            descTemplate.bytesPerTexel = GetBytesPerTexel();
            descTemplate.srcRowPitch = GetRowPitchInBytes();
            descTemplate.startCol = 0;
            descTemplate.endCol = GetWidth();
            descTemplate.startRow = 0;
            descTemplate.endRow = GetHeight();

            if (totalThreads > 0)
            {
                size_t rowsPerThread = GetHeight() / totalThreads;
                for (uint8_t threadNum = 0; threadNum < totalThreads; threadNum++)
                {

                    threads[threadNum] = std::thread
                    (
                        [&descTemplate,rowsPerThread, threadNum]()
                    {
                        PixelUtil::TransformTexelsInfo desc = descTemplate;
                        desc.startRow = rowsPerThread * threadNum;
                        desc.endRow = rowsPerThread * (threadNum + 1);
                        PixelUtil::TransformTexels(desc);
                    }
                    );
                }

                PixelUtil::TransformTexelsInfo desc = descTemplate;
                desc.startRow = rowsPerThread * totalThreads;
                desc.endRow = GetHeight();

                PixelUtil::TransformTexels(desc);

                for (uint8_t i = 0; i < totalThreads; i++)
                    threads[i].join();
            }
            else
            {
                // single (main) thread implementation
                PixelUtil::TransformTexels(descTemplate);
            }

            if (transform == AAT_Rotate90CW || transform == AAT_Rotate90CCW)
                std::swap(fProperies.Height, fProperies.Width);

            fProperies.RowPitchInBytes = fProperies.Width * GetBytesPerTexel();

            delete[]fProperies.ImageBuffer;
            fProperies.ImageBuffer = dest;
        }
    }
}
