#pragma once
#include <cstdint>
#include <thread>
#include <algorithm>
#include "AxisAlignedTransform.h"
#include <ExoticNumbers/Int24.h>
#include <LLUtils/Exception.h>
#include <LLUtils/Warnings.h>

namespace IMUtil
{
    typedef void(*PixelConvertFunc)(std::byte* i_dest, const std::byte* i_src, size_t start, size_t end);

#pragma pack(push,1)

    class PixelUtil
    {
    public:
        struct alignas(1) BitTexel8 { uint8_t X; };

        struct alignas(1) BitTexel16 : public BitTexel8 { uint8_t Y; };

        struct alignas(1) BitTexel24 : public BitTexel16 { uint8_t Z; };

        struct alignas(1) BitTexel32 : public BitTexel24 { uint8_t W; };


LLUTILS_DISABLE_WARNING_PUSH           

LLUTILS_DISABLE_WARNING_ANONYMOUS_STRUCT
LLUTILS_DISABLE_WARNING_NESTED_ANNONYMOUS_TYPES

        struct alignas(1) BitTexel32Ex
        {
            union
            {
                uint32_t value;
                struct
                {
                    uint8_t X;
                    uint8_t Y;
                    uint8_t Z;
                    uint8_t W;
                };
            };
        };
LLUTILS_DISABLE_WARNING_POP
#pragma pack(pop)

        // A function to copy a same format texel from one place to another
        static void CopyTexel(void* dest, const std::size_t idxDest, const void* src, const std::size_t idxSrc, uint8_t texelSize)
        {
            memcpy(reinterpret_cast<uint8_t*>(dest) + idxDest * texelSize
                , reinterpret_cast<const uint8_t*>(src) + idxSrc * texelSize
                , texelSize);
        }

        static void Convert(PixelConvertFunc convertFunc, std::byte** i_dest, const std::byte* i_src, const uint8_t dstTexelSizeinBits, const size_t numTexels)
        {
            using namespace std;
            //TODO: fine tune the minimum size required to open helper threads
            const size_t MegaBytesPerThread = 6;
            static const uint8_t MaxGlobalThrads = 32;
            static uint8_t maxThreads = static_cast<uint8_t>(
                min(static_cast<unsigned int>(MaxGlobalThrads), max(1u, thread::hardware_concurrency() - 1)));
            static std::thread threads[MaxGlobalThrads];


            const size_t bytesPerThread = MegaBytesPerThread * 1024 * 1024;
            const size_t bytesPerPixel = dstTexelSizeinBits / 8;
            const size_t texelsPerThread = bytesPerThread / bytesPerPixel;
            
            const uint8_t totalThreads = (std::min)(maxThreads, static_cast<uint8_t>(numTexels / texelsPerThread));

            std::byte* dest = *i_dest;
            if (totalThreads > 0)
            {
                const size_t segmentSize = numTexels / (totalThreads + 1);

                for (uint8_t threadNum = 0; threadNum < totalThreads; threadNum++)
                {
                    threads[threadNum] = std::thread
                    (
                        [convertFunc, threadNum, i_src, dest, segmentSize]()
                    {

                        const size_t start = threadNum * segmentSize;
                        const size_t end = start + segmentSize;
                        convertFunc(dest, i_src, start, end);
                    }
                    );
                }

                const size_t start = (totalThreads)* segmentSize;
                const size_t end = start + segmentSize;
                convertFunc(dest, i_src, start, end);

                for (uint8_t i = 0; i < totalThreads; i++)
                    threads[i].join();
            }
            else
            {
                // single threaded implementation.
                convertFunc(dest, i_src, 0, numTexels);
            }

        }

        static void BGR24ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            uint32_t* dst = (uint32_t*)i_dest;
            BitTexel24 * src = (BitTexel24*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i] = 0xFF << 24 | (src[i].Z << 0) | (src[i].Y << 8) | (src[i].X << 16);
            }
        }


        static void RGB24ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            uint32_t* dst = (uint32_t*)i_dest;
            BitTexel24 * src = (BitTexel24*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i] = 0xFF << 24 | (src[i].Z << 16) | (src[i].Y << 8) | (src[i].X << 0);
            }
        }


        static void ABGR32ToBGRA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            BitTexel32* dst = (BitTexel32*)i_dest;
            BitTexel32 * src = (BitTexel32*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i].X = src[i].Y;
                dst[i].Y = src[i].Z;
                dst[i].Z = src[i].W;
                dst[i].W = src[i].X;

            }
        }

        static void RGB48ToBGRA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
#pragma pack (push,1)
            struct RGB48
            {
                uint16_t R;
                uint16_t G;
                uint16_t B;
            };

#pragma pack (pop)
            BitTexel32* dst = (BitTexel32*)i_dest;
            RGB48* src = (RGB48*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i].X = static_cast<uint8_t>(src[i].R >> 8);
                dst[i].Y = static_cast<uint8_t>(src[i].G >> 8);
                dst[i].Z = static_cast<uint8_t>(src[i].B >> 8);
                dst[i].W = 255;
            }
        }
        

        static void ABGR32ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            BitTexel32* dst = (BitTexel32*)i_dest;
            BitTexel32 * src = (BitTexel32*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i].X = src[i].W;
                dst[i].Y = src[i].Z;
                dst[i].Z = src[i].Y;
                dst[i].W = src[i].X;

            }
        }


        static void ARGB32ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            BitTexel32* dst = (BitTexel32*)i_dest;
            BitTexel32 * src = (BitTexel32*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i].X = src[i].Y;
                dst[i].Y = src[i].Z;
                dst[i].Z = src[i].W;
                dst[i].W = src[i].X;
            }
        }

        static void BGRA32ToBGR24(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            BitTexel24* dst = (BitTexel24*)i_dest;
            BitTexel32* src = (BitTexel32*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i].X = src[i].X;
                dst[i].Y = src[i].Y;
                dst[i].Z = src[i].Z;
            }
        }

        

        static void RGBA32ToBGRA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            BitTexel32* dst = (BitTexel32*)i_dest;
            BitTexel32 * src = (BitTexel32*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i].X = src[i].Z;
                dst[i].Y = src[i].Y;
                dst[i].Z = src[i].X;
                dst[i].W = src[i].W;
            }
        }


        static void A8ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            BitTexel32* dst = (BitTexel32*)i_dest;
            BitTexel8 * src = (BitTexel8*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i].X = 255;
                dst[i].Y = 255;
                dst[i].Z = 255;
                dst[i].W = src[i].X;
            }
        }

        static void A1ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            BitTexel32* dst = (BitTexel32*)i_dest;
            for (size_t i = start; i < end; i++)
            {
                const size_t byteOffset = i / CHAR_BIT;
                const size_t bitOffset = i % CHAR_BIT;
                //OutputDebugString(byteOffset)
                uint8_t currentByte = (reinterpret_cast<const uint8_t*>(i_src))[byteOffset];
                uint8_t color = ((currentByte & (1 << (7 - bitOffset))) != 0) ? 255 : 0;

                dst[i].X = color;
                dst[i].Y = color;
                dst[i].Z = color;
                dst[i].W = 255;
            }
        }

        static void A16ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            BitTexel32* dst = (BitTexel32*)i_dest;
            uint16_t* src = (uint16_t*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i].X = src[i] >> 8;
                dst[i].Y = src[i] >> 8;
                dst[i].Z = src[i] >> 8;
                dst[i].W = 255;
            }
        }

        static void BGR16ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
#pragma pack(push,1)
            struct RGB5551
            {
                unsigned short B : 5;
                unsigned short G : 5;
                unsigned short R : 5;
                unsigned short X : 1;
            };

       
#pragma pack(pop)

            static_assert(sizeof(RGB5551) == 2, " 16 bit struct size mismatch");

            BitTexel32* dst = (BitTexel32*)i_dest;
            uint16_t* src = (uint16_t*)i_src;

            for (size_t i = start; i < end; i++)
            {
                RGB5551& rgb16 = reinterpret_cast<RGB5551&>(src[i]);

                dst[i].X = (rgb16.R << 3) | 0x7;
                dst[i].Y = (rgb16.G << 3) | 0x7;
                dst[i].Z = (rgb16.B << 3) | 0x7;
                dst[i].W = 255;
            }
        }


        static void RGBF32ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {

#pragma pack(push,1)
            struct RGBF32
            {
                float r, g, b;
            };
#pragma pack(pop)

            BitTexel32* dst = (BitTexel32*)i_dest;
            const RGBF32* src = reinterpret_cast<const RGBF32 *>(i_src);

            for (size_t i = start; i < end; i++)
            {
                const RGBF32& rgbF32 = src[i];
                dst[i].X = static_cast<uint8_t>(rgbF32.r * 255);
                dst[i].Y = static_cast<uint8_t>(rgbF32.g * 255);
                dst[i].Z = static_cast<uint8_t>(rgbF32.b * 255);
                dst[i].W = 255;
            }
        }


        static void BGR565ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
#pragma pack(push,1)
            struct RGB565
            {
                unsigned short B : 5;
                unsigned short G : 6;
                unsigned short R : 5;
            };
#pragma pack(pop)

            static_assert(sizeof(RGB565) == 2, " 16 bit struct size mismatch");

            BitTexel32* dst = (BitTexel32*)i_dest;
            uint16_t* src = (uint16_t*)i_src;

            for (size_t i = start; i < end; i++)
            {
                RGB565& rgb16 = reinterpret_cast<RGB565&>(src[i]);
                dst[i].X = (rgb16.R << 3) | 0x7;
                dst[i].Y = (rgb16.G << 2) | 0x3;
                dst[i].Z = (rgb16.B << 3) | 0x7;
                dst[i].W = 255;
            }
        }


        static void BGRA32ToRGBA32(std::byte* i_dest, const std::byte* i_src, std::size_t start, std::size_t end)
        {
            BitTexel32* dst = (BitTexel32*)i_dest;
            BitTexel32 * src = (BitTexel32*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i].X = src[i].Z;
                dst[i].Y = src[i].Y;
                dst[i].Z = src[i].X;
                dst[i].W = src[i].W;
            }
        }

        struct AxisAlignedResolvedTransformData
        {
            uint8_t flipX = 0;
            uint8_t flipY = 0;
            uint8_t switchDimensions = 0;
            uint8_t reserved = 0;
        };

        struct TransformTexelsInfo
        {
            size_t startRow{};
            size_t endRow{};
            size_t startCol{};
            size_t endCol{};
            size_t width{};
            size_t height{};
            const std::byte* srcBuffer{};
            std::byte* dstBuffer{};
            AxisAlignedResolvedTransformData resolvedTransformData;
            size_t srcRowPitch{};
            size_t bytesPerTexel{};
        };


    

        static AxisAlignedResolvedTransformData ReolvedAxisAlignedTransformData(OIV_AxisAlignedTransform transform)
        {
            AxisAlignedResolvedTransformData transformData;

            transformData.flipX = (((int)transform.flip & (int)OIV_AxisAlignedFlip::Horizontal) != 0) ? 1 : 0;
            transformData.flipY = (((int)transform.flip & (int)OIV_AxisAlignedFlip::Vertical) != 0) ? 1 : 0;

            //if rotate is 90 counter clock wise flip horizontal axis
            switch (transform.rotation)
            {
            case OIV_AxisAlignedRotation::Rotate180:
                transformData.flipX ^= 1;
                transformData.flipY ^= 1;
                break;
            case OIV_AxisAlignedRotation::Rotate90CCW:
                transformData.flipY ^= 1;
                transformData.switchDimensions = 1;
                break;
            case OIV_AxisAlignedRotation::Rotate90CW:
                transformData.flipX ^= 1;
                transformData.switchDimensions = 1;
                break;
            case OIV_AxisAlignedRotation::None:
                break;
            }


            return transformData;
        }
        

        static void TransformTexels(const TransformTexelsInfo& transformInfo)
        {

            const AxisAlignedResolvedTransformData& resolvedTransform = transformInfo.resolvedTransformData;

            for (std::size_t y = transformInfo.startRow; y < transformInfo.endRow; y++)
                for (std::size_t x = transformInfo.startCol; x < transformInfo.endCol; x++)
                {
                    const std::byte* srcRow = transformInfo.srcBuffer + y * transformInfo.srcRowPitch;

                    size_t destWidth = transformInfo.width;
                    size_t destHeight = transformInfo.height;
                    size_t destX = x;
                    size_t destY = y;

                    if (resolvedTransform.switchDimensions == 1)
                    {
                        destWidth = transformInfo.height;
                        destHeight = transformInfo.width;
                        destX = y;
                        destY = x;
                    }

                    if (resolvedTransform.flipX)
                        destX = destWidth - destX - 1;

                    if (resolvedTransform.flipY)
                        destY = destHeight - destY - 1;

                    const std::size_t idxDest =  destY * destWidth + destX;

                    PixelUtil::CopyTexel(transformInfo.dstBuffer, idxDest, srcRow, x, static_cast<uint8_t>(transformInfo.bytesPerTexel));
                }
        }

        //static void BGR32ToRGBA32(uint8_t** i_dest, uint8_t* i_src, size_t size)
        //{
        //    if (size % 3 != 0)
        //        throw std::logic_error("Data is not aligned");

        //    size_t totalTexels = size / 3;
        //    *i_dest = new uint8_t[totalTexels * 4];
        //    StopWatch stopWatch;
        //    
        //    //Simd solution, try to find away to set the alpha channel to 255 instead of 0
        //  /*  __m128i *src = (__m128i *)i_src;
        //    __m128i *dst = (__m128i *)*i_dest;
        //    __m128i mask = _mm_setr_epi8(0, 1, 2, -1, 3, 4, 5, -1, 6, 7, 8, -1, 9, 10, 11, -1);

        //    for (int i = 0; i<totalTexels; i += 16) {
        //        __m128i sa = _mm_load_si128(src);
        //        __m128i sb = _mm_load_si128(src + 1);
        //        __m128i sc = _mm_load_si128(src + 2);
        //        
        //        __m128i val = _mm_shuffle_epi8(sa, mask);
        //        _mm_store_si128(dst, val);
        //        val = _mm_shuffle_epi8(_mm_alignr_epi8(sb, sa, 12), mask);
        //        _mm_store_si128(dst + 1, val);
        //        val = _mm_shuffle_epi8(_mm_alignr_epi8(sc, sb, 8), mask);
        //        _mm_store_si128(dst + 2, val);
        //        val = _mm_shuffle_epi8(_mm_alignr_epi8(sc, sc, 4), mask);
        //        _mm_store_si128(dst + 3, val);

        //        src += 3;
        //        dst += 4;
        //    }
    };
}

