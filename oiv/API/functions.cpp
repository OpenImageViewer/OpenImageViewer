#include "..\PreCompiled.h"
#include "functions.h"

int __cdecl OIV_Execute(int command, size_t requestSize, void* requestData, size_t responseSize, void* responseData)
{

    return OIV::Execute_impl(static_cast<CommandExecute>(command), requestSize, requestData, responseSize, responseData);
}