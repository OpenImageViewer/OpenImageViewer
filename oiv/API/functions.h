#pragma once
#include "..\ApiImpl.h"
#include "defs.h"
#include "..\Common/linux.h"

extern "C"
{
    OIV_EXPOSE_FUNCTION ResultCode __cdecl OIV_Execute(int command, std::size_t requestSize, void* requestData, std::size_t responseSize, void* responseData);
    OIV_EXPOSE_FUNCTION ResultCode __cdecl OIV_Util_GetBPPFromTexelFormat(OIV_TexelFormat in_texelFormat, uint8_t* out_bpp);
}
