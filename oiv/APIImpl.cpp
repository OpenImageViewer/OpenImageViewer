#include "APIImpl.h"
#include "Commands/CommandProcessor.h"
#include "ApiGlobal.h"

namespace OIV
{

    ResultCode Execute_impl(int command, std::size_t requestSize, void* requestData, std::size_t responseSize, void* responseData)
    {
        return ApiGlobal::sCommandProcessor.ProcessCommand(static_cast<CommandExecute>(command), requestSize, requestData, responseSize, responseData);
    }
}

