#pragma once
#include <cstdint>
#include <stdexcept>

namespace IMCodec
{
    // I - unsigned integer
    // F - float
    // R - red
    // B - blue
    // G - green
    // A - alpha (opacity)
    // X - any other channel (depth, gray-scale, monochrome or luminance)

    enum TexelFormat : uint16_t
    {
          TF_BEGIN
        , TF_UNKNOWN = TF_BEGIN
        , TF_I_R8_G8_B8
        , TF_I_R8_G8_B8_A8
        , TF_I_B8_G8_R8
        , TF_I_B8_G8_R8_A8
        , TF_I_A8_R8_G8_B8
        , TF_I_A8_B8_G8_R8
        , TF_I_X1
        , TF_I_X8
        , TF_F_X16
        , TF_F_X24
        , TF_F_X32
        , TF_F_X64
        , TF_COUNT
    };

    const uint8_t TexelFormatSize[] =
        {
             0       // TF_UNKNOWN
            ,24      // TF_I_R8_G8_B8
            ,32      // TF_I_R8_G8_B8_A8
            ,24      // TF_I_B8_G8_R8
            ,32      // TF_I_B8_G8_R8_A8
            ,32      // TF_I_A8_R8_G8_B8
            ,32      // TF_I_A8_B8_G8_R8
            ,1       // TF_I_X1
            ,8       // TF_I_X8
            ,16      // TF_F_X16
            ,24      // TF_F_X24
            ,32      // TF_F_X32
            ,64      // TF_F_X64
        };         

    __forceinline  uint8_t GetTexelFormatSize(TexelFormat format)
    {
        static_assert(sizeof(TexelFormatSize) / sizeof(TexelFormatSize[0]) == TF_COUNT, " Wrong array size");

        if (format >= TF_BEGIN && format < TF_COUNT)
            return TexelFormatSize[format];
        else
            throw std::logic_error("Index out of bounds");
    }
}
