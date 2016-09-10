#include "..\PreCompiled.h"
#include "functions.h"

int __cdecl OIV_Execute(int command, size_t commandSize, void* commandData)
{
    return OIV::Execute_impl((CommandExecute) command, commandSize, commandData);
}
int __cdecl OIV_Query(int command, void* commandData, size_t commandSize, void* output_data, size_t output_size)
{
    return OIV::Query_impl((CommandQuery) command, commandData, commandSize, output_data, output_size);
}