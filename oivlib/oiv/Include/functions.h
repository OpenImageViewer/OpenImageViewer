#pragma once
#include <LLUtils/Platform.h>
#include "defs.h"

#ifdef OIV_LIBRARY_BUILD 
    #define OIV_EXPORT LLUTILS_EXPORT
#else
    #define OIV_EXPORT LLUTILS_IMPORT
#endif

extern "C"
{
    OIV_EXPORT ResultCode OIV_Execute(int command, std::size_t requestSize, void* requestData, std::size_t responseSize, void* responseData);
    OIV_EXPORT ResultCode OIV_Util_GetBPPFromTexelFormat(OIV_TexelFormat in_texelFormat, uint8_t* out_bpp);
}
