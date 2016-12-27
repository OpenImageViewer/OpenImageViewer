#pragma once
#include <cstdint>
#include <stdexcept>

namespace OIV
{
    class PixelUtil
    {
    public:
        struct alignas(1) BitTexel8 { uint8_t X; };

        struct alignas(1) BitTexel16 : public BitTexel8 { uint8_t Y; };

        struct alignas(1) BitTexel24 : public BitTexel16 { uint8_t Z; };

        struct alignas(1) BitTexel32 : public BitTexel24 { uint8_t W; };


        // A function to copy a same format texel from one place to another
        template <class BIT_TEXEL_TYPE>
        static __forceinline  void CopyTexel(void* dest, const std::size_t idxDest, const void* src, const std::size_t idxSrc)
        {
            reinterpret_cast<BIT_TEXEL_TYPE*>(dest)[idxDest] = reinterpret_cast<const  BIT_TEXEL_TYPE*>(src)[idxSrc];

        }

        static void RGB24ToRGBA32(uint8_t** dest, uint8_t* src, std::size_t size)
        {
            if (size % 3 != 0)
                throw std::logic_error("Data is not aligned");

            std::size_t totalElements = size / 3;

            *dest = new uint8_t[totalElements * 4];

            BitTexel32* destPos = (BitTexel32*)*dest;
            BitTexel24* sourcePos = (BitTexel24*)src;

            for (int i = 0; i < totalElements; i++)
            {
                const BitTexel24& sourcetexel = *sourcePos;
                BitTexel32& destTexel = *destPos;
                destTexel.X = sourcetexel.Z;
                destTexel.Y = sourcetexel.Y;
                destTexel.Z = sourcetexel.X;
                destTexel.W = 255;
                destPos++;
                sourcePos++;
            }
        }

        static void BGR24ToRGBA32(uint8_t** dest, uint8_t* src, std::size_t size)
        {
            if (size % 3 != 0)
                throw std::logic_error("Data is not aligned");

            std::size_t totalElements = size / 3;

            *dest = new uint8_t[totalElements * 4];

            BitTexel32* destPos = (BitTexel32*)*dest;
            BitTexel24* sourcePos = (BitTexel24*)src;

            for (int i = 0; i < totalElements; i++)
            {
                const BitTexel24& sourcetexel = *sourcePos;
                BitTexel32& destTexel = *destPos;
                destTexel.X = sourcetexel.X;
                destTexel.Y = sourcetexel.Y;
                destTexel.Z = sourcetexel.Z;
                destTexel.W = 255;
                destPos++;
                sourcePos++;
            }
        }
    };
}

