#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerRefresh : public CommandHandler
    {
    protected:

        ResultCode ExecuteImpl([[maybe_unused]] const void* request, [[maybe_unused]] const std::size_t requestSize, [[maybe_unused]] void* response, [[maybe_unused]] const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            result = (ResultCode)ApiGlobal::sPictureRenderer->Refresh();
            return result;
        }
    };


}

