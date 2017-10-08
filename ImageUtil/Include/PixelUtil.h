#pragma once
#include <cstdint>
#include <stdexcept>
#include <thread>
#include <algorithm>
#include "AxisAlignedTransform.h"
#include "Int24.h"

namespace IMUtil
{
    typedef void(*PixelConvertFunc)(uint8_t* i_dest, const uint8_t* i_src, size_t start, size_t end);
    class PixelUtil
    {
    public:
        struct alignas(1) BitTexel8 { uint8_t X; };

        struct alignas(1) BitTexel16 : public BitTexel8 { uint8_t Y; };

        struct alignas(1) BitTexel24 : public BitTexel16 { uint8_t Z; };

        struct alignas(1) BitTexel32 : public BitTexel24 { uint8_t W; };

#pragma pack(1)

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
                struct
                {
                    Int24 XYZ;
                    uint8_t W;
                };
            };
            
        };
#pragma pack()

        // A function to copy a same format texel from one place to another
        template <class BIT_TEXEL_TYPE>
        static void CopyTexel(void* dest, const std::size_t idxDest, const void* src, const std::size_t idxSrc)
        {
            reinterpret_cast<BIT_TEXEL_TYPE*>(dest)[idxDest] = reinterpret_cast<const  BIT_TEXEL_TYPE*>(src)[idxSrc];

        }

        static void Convert(PixelConvertFunc convertFunc, uint8_t** i_dest, const uint8_t* i_src, const uint8_t dstTexelSizeinBits, const size_t numTexels)
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
            *i_dest = new uint8_t[numTexels * bytesPerPixel];
            uint8_t* dest = *i_dest;
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

        static void BGR24ToRGBA32(uint8_t* i_dest, const uint8_t* i_src, std::size_t start, std::size_t end)
        {
            uint32_t* dst = (uint32_t*)i_dest;
            BitTexel24 * src = (BitTexel24*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i] = 0xFF << 24 | (src[i].Z << 0) | (src[i].Y << 8) | (src[i].X << 16);
            }
        }


        static void RGB24ToRGBA32(uint8_t* i_dest, const uint8_t* i_src, std::size_t start, std::size_t end)
        {
            uint32_t* dst = (uint32_t*)i_dest;
            BitTexel24 * src = (BitTexel24*)i_src;

            for (size_t i = start; i < end; i++)
            {
                dst[i] = 0xFF << 24 | (src[i].Z << 16) | (src[i].Y << 8) | (src[i].X << 0);
            }
        }

        static void RGBA32ToBGRA32(uint8_t* i_dest, const uint8_t* i_src, std::size_t start, std::size_t end)
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


        static void A8ToRGBA32(uint8_t* i_dest, const uint8_t* i_src, std::size_t start, std::size_t end)
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

        

        

        static void BGRA32ToRGBA32(uint8_t* i_dest, const uint8_t* i_src, std::size_t start, std::size_t end)
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

        struct TransformTexelsInfo
        {
            size_t startRow;
            size_t endRow;
            size_t startCol;
            size_t endCol;
            size_t width;
            size_t height;
            uint8_t* srcBuffer;
            uint8_t* dstBuffer;
            IMUtil::AxisAlignedRTransform transform;
            size_t srcRowPitch;
            size_t bytesPerTexel;
        };

        static void TransformTexels(const TransformTexelsInfo& transformInfo)
        {
            for (std::size_t y = transformInfo.startRow; y < transformInfo.endRow; y++)
                for (std::size_t x = transformInfo.startCol; x < transformInfo.endCol; x++)
                {
                    const uint8_t* srcRow = transformInfo.srcBuffer + y * transformInfo.srcRowPitch;
                    std::size_t idxDest;

                    switch (transformInfo.transform)
                    {
                    case AAT_Rotate180:
                        idxDest = transformInfo.width - x - 1 + (transformInfo.height - y - 1) * transformInfo.width;
                        break;
                    case AAT_Rotate90CW:
                        idxDest = (transformInfo.height - 1 - y) + x * transformInfo.height;
                        break;
                    case AAT_Rotate90CCW:
                        idxDest = y + (transformInfo.width - 1 - x) * transformInfo.height;
                        break;
                    case AAT_FlipVertical:
                        idxDest = x + (transformInfo.height - y - 1) * transformInfo.width;
                        break;
                    case AAT_FlipHorizontal:
                        idxDest = (transformInfo.width - 1 - x) + y * transformInfo.width;
                        break;

                    default:
                        throw std::runtime_error("Wrong or corrupted value");
                    }



                    switch (transformInfo.bytesPerTexel)
                    {
                    case 1:

                        PixelUtil::CopyTexel <PixelUtil::BitTexel8>(transformInfo.dstBuffer, idxDest, srcRow, x);
                        break;
                    case 2:
                        PixelUtil::CopyTexel<PixelUtil::BitTexel16>(transformInfo.dstBuffer, idxDest, srcRow, x);
                        break;
                    case 3:
                        PixelUtil::CopyTexel<PixelUtil::BitTexel24>(transformInfo.dstBuffer, idxDest, srcRow, x);
                        break;
                    case 4:
                        PixelUtil::CopyTexel<PixelUtil::BitTexel32>(transformInfo.dstBuffer, idxDest, srcRow, x);
                        break;
                    default:
                        throw std::runtime_error("Wrong or corrupted value");
                    }
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

