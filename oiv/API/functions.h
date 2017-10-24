#pragma once
#include "..\ApiImpl.h"
#include "defs.h"

#ifdef _MSC_VER
#define OIV_FORCE_INLINE __forceinline
#else
#define OIV_FORCE_INLINE __attribute__((always_inline))
#endif


#if OIV_CLIENT_BUILD == 1
#define OIV_EXPOSE_FUNCTION __declspec( dllimport )
#else
#define OIV_EXPOSE_FUNCTION __declspec(dllexport)
#endif

extern "C"
{
    OIV_EXPOSE_FUNCTION ResultCode __cdecl OIV_Execute(int command, std::size_t requestSize, void* requestData, std::size_t responseSize, void* responseData);
    OIV_EXPOSE_FUNCTION ResultCode __cdecl OIV_Util_GetBPPFromTexelFormat(OIV_TexelFormat in_texelFormat, uint8_t* out_bpp);
}
