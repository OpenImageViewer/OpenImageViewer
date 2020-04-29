#pragma once
#include <defs.h>
namespace OIV
{
    ResultCode Execute_impl(int command, std::size_t requestSize, void* requestData, std::size_t responseSize, void* responseData);
    namespace Util
    {
        ResultCode GetBPPFromTexelFormat_impl(OIV_TexelFormat in_texelFormat, uint8_t* out_bpp);
    }
    
}