#pragma once
#include "..\ApiImpl.h"
#include "defs.h"
extern "C"
{
    OIV_EXPOSE_FUNCTION int __cdecl OIV_Execute(int command, size_t commandSize, void* commandData);
    OIV_EXPOSE_FUNCTION int __cdecl OIV_Query(int command, void* commandData, size_t commandSize, void* output_data, size_t output_size);
}