#include "APIImpl.h"
#include "CommandProcessor.h"

namespace OIV
{

    ResultCode Execute_impl(int command, std::size_t requestSize, void* requestData, std::size_t responseSize, void* responseData)
    {
        return CommandProcessor::ProcessCommand(static_cast<CommandExecute>(command), requestSize, requestData, responseSize, responseData);
    }
}

