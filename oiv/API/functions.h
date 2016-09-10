#pragma once
#include "..\ApiImpl.h"
#include "defs.h"
extern "C"
{
    OIV_EXPOSE_FUNCTION int __cdecl OIV_Execute(int command, size_t requestSize, void* requestData, size_t responseSize, void* responseData);
}