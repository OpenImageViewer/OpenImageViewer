#include "TexelConvertor.h"
#include <System.h>
#include <algorithm>
#include <thread>
namespace OIV
{

    TexelConvertor::SwizzleFuncType TexelConvertor::FindSwizzleFunc(IMCodec::TexelFormat sourcePixelFormat, IMCodec::TexelFormat targetPixelFormat)
    {
        using namespace IMCodec;
        ConvertKey key(sourcePixelFormat, targetPixelFormat);
        auto it = fConversionMappings.find(key);

        if (it != fConversionMappings.end())
        {
            return it->second;
        }
        else
        {
            SwizzleFuncType swizzleFunc = PopulateSwizzleFunc(sourcePixelFormat, targetPixelFormat);
            it = fConversionMappings.emplace(key, swizzleFunc).first;
        }

        return it != fConversionMappings.end() ? it->second : nullptr;
    }

    bool TexelConvertor::Convert(IMCodec::TexelFormat targetPixelFormat, IMCodec::TexelFormat sourcePixelFormat,
        std::byte* destBuffer, const std::byte* sourceBuffer, size_t numpixels)
    {
        {
            auto swizzleFunc = FindSwizzleFunc(sourcePixelFormat, targetPixelFormat);
            using namespace IMCodec;

            if (swizzleFunc != nullptr)
            {
                using namespace std;
                //TODO: fine tune the minimum size required to open helper threads
                const size_t MaxMegaBytesPerThread = 6;
                const size_t MaxBytesPerThread = MaxMegaBytesPerThread * 1024 * 1024;
                static const uint8_t MaxGlobalThrads = 32;
                static const uint8_t maxThreads = static_cast<uint8_t>(
                    min(static_cast<unsigned int>(MaxGlobalThrads), max(1u, System::GetIdealNumThreadsForMemoryOperations())));

                static thread_local std::thread threads[MaxGlobalThrads];

                const size_t sourceTexelSize = GetTexelFormatSize(sourcePixelFormat) / CHAR_BIT;
                const size_t DestTexellSize = GetTexelFormatSize(targetPixelFormat) / CHAR_BIT;

                const size_t totalTexels = numpixels;
                const uint8_t totalThreads = std::min<int>(maxThreads, static_cast<int>(totalTexels * std::max(DestTexellSize, sourceTexelSize)) / MaxBytesPerThread);


                if (totalThreads > 0)
                {
                    const size_t texelsPerThread = totalTexels / totalThreads;
                    for (uint8_t threadNum = 0; threadNum < totalThreads; threadNum++)
                    {
                        threads[threadNum] = std::thread([&, threadNum]()
                            {
                                const size_t baseTexel = threadNum * texelsPerThread;
                                const std::byte* sourceAddress = reinterpret_cast<const std::byte*>
                                    (reinterpret_cast<const uint8_t*>(sourceBuffer) + (sourceTexelSize * baseTexel));

                                std::byte* targetAddress = reinterpret_cast< std::byte*>
                                    (reinterpret_cast<uint8_t*>(destBuffer) + (DestTexellSize * baseTexel));

                                ConvertSegment(targetAddress, sourceAddress, texelsPerThread, sourceTexelSize, DestTexellSize, swizzleFunc);
                            }
                        );
                    }


                    //Convert the last data in the image in case there's any.
                    
                    const size_t baseTexel = totalThreads * texelsPerThread;
                    const std::byte* sourceAddress = reinterpret_cast<const std::byte*>
                        (reinterpret_cast<const uint8_t*>(sourceBuffer) + (sourceTexelSize * baseTexel));

                    std::byte* targetAddress = reinterpret_cast<std::byte*>
                        (reinterpret_cast<uint8_t*>(destBuffer) + (DestTexellSize * baseTexel));
                    
                    ConvertSegment(targetAddress, sourceAddress, totalTexels % totalThreads , sourceTexelSize, DestTexellSize, swizzleFunc);


                    for (uint8_t i = 0; i < totalThreads; i++)
                        threads[i].join();
                }
                else
                {
                    ConvertSegment(destBuffer
                        , sourceBuffer
                        , numpixels
                        , GetTexelFormatSize(sourcePixelFormat) / CHAR_BIT
                        , GetTexelFormatSize(targetPixelFormat) / CHAR_BIT, swizzleFunc);
                }
                return true;
            }
            else
            {
                return false;
            }
        }
    }


    TexelConvertor::SwizzleFuncType TexelConvertor::PopulateSwizzleFunc(IMCodec::TexelFormat sourcePixelFormat, IMCodec::TexelFormat targetPixelFormat)
    {
        return nullptr;
        //LL_EXCEPTION_NOT_IMPLEMENT("Auto generation of convertion swizzle function is yet to be implemented");
    }
}