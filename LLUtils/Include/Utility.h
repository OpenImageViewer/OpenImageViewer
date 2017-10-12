#pragma once
#include <list>
#include <filesystem>

#include "StringUtility.h"


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
        static int8_t Sign(T val)
        {
            return (T(0) < val) - (val < T(0));
        }

        static bool EnsureDirectory(const string_type& path)
        {
            if (std::experimental::filesystem::exists(path) == false)
                return std::experimental::filesystem::create_directory(path);

            return std::experimental::filesystem::is_directory(path);
        }

        struct BlitBox
        {
            uint8_t* buffer;
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
            const uint8_t* srcPos = src.buffer + src.GetStartOffset();
            uint8_t* dstPos = dst.buffer + dst.GetStartOffset();

            const uint32_t bytesPerCopy = src.pixelSizeInbytes * src.width;

            for (uint32_t y = src.top; y < src.height; y++)
            {
                memcpy(dstPos, srcPos, bytesPerCopy);
                dstPos += dst.rowPitch;
                srcPos += src.rowPitch;
            }
        }

        template <class T> static  T Clamp(T val, T min, T max)
        {
            return std::max(min, std::min(val, max));
        }
    
    };
}