#pragma once
#include <filesystem>

namespace LLUtils
{

    class Utility
    {
    public:
        template <class T>
        static T Align(T num ,T alignement)
        {
            static_assert(std::is_integral<T>(),"Alignment works only with integrals");
            if (alignement < 1)
                throw std::logic_error("alignement must be a positive value");
            return (num + (alignement - 1)) / alignement * alignement;
        }
        

        template <typename T>
        static T Sign(T val)
        {
            return (T(0) < val) - (val < T(0));
        }


        struct BlitBox
        {
            std::byte* buffer;
            uint32_t rowPitch;
            uint32_t width;
            uint32_t height;
            uint32_t left;
            uint32_t top;
            uint32_t pixelSizeInbytes;
            
            int32_t GetStartOffset() const
            {
                return (top * rowPitch) + (left * pixelSizeInbytes);
                
            }


        };

        static void Blit(BlitBox& dst, const BlitBox& src)
        {
            const std::byte* srcPos = src.buffer + src.GetStartOffset();
            std::byte* dstPos = dst.buffer + dst.GetStartOffset();

            const uint32_t bytesPerCopy = src.pixelSizeInbytes * src.width;

            for (uint32_t y = src.top; y < src.height; y++)
            {
                memcpy(dstPos, srcPos, bytesPerCopy);
                dstPos += dst.rowPitch;
                srcPos += src.rowPitch;
            }
        }

      
    };
}