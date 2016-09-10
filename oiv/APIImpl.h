#pragma once
#include "API\defs.h"
#include "CommandProcessor.h"
namespace OIV
{
    inline ResultCode Execute_impl(CommandExecute command, size_t commandSize,void* commandData)
    {
        return CommandProcessor::ProcessCommand(command, commandSize, commandData);
    }

    inline ResultCode Query_impl(CommandQuery command, void* commandData, size_t commandSize, void* output_data, size_t output_size)
    {
        return CommandProcessor::ProcessQuery(command, commandData, commandSize, output_data, output_size);
    }
}
void

