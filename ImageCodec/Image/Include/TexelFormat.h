#pragma once
#include <cstdint>
#include "Exception.h"

namespace IMCodec
{
    // I - unsigned integer
    // F - float
    // R - red
    // B - blue
    // G - green
    // A - alpha (opacity)
    // X - any other channel (depth, gray-scale, monochrome or luminance)

    enum class TexelFormat : uint16_t
    {
          BEGIN
        , UNKNOWN = BEGIN
        , I_R8_G8_B8
        , I_R8_G8_B8_A8
        , I_B8_G8_R8
        , I_B8_G8_R8_A8
        , I_A8_R8_G8_B8
        , I_A8_B8_G8_R8
        , I_A8
        , I_X1
        , I_X8
        , F_X16
        , F_X24
        , F_X32
        , F_X64
        , COUNT
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
            ,8       // TF_I_A8
            ,1       // TF_I_X1
            ,8       // TF_I_X8
            ,16      // TF_F_X16
            ,24      // TF_F_X24
            ,32      // TF_F_X32
            ,64      // TF_F_X64
        };         

    __forceinline  uint8_t GetTexelFormatSize(TexelFormat format)
    {
        static_assert(sizeof(TexelFormatSize) / sizeof(TexelFormatSize[0]) == static_cast<std::underlying_type<TexelFormat>::type>(TexelFormat::COUNT), " Wrong array size");
        
        if (format >= TexelFormat::BEGIN && format < TexelFormat::COUNT)
            return TexelFormatSize[static_cast<std::underlying_type<TexelFormat>::type>(format)];
        else
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::LogicError, " index out of bounds.");
    }
}
