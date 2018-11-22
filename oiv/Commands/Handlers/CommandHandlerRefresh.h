#pragma once
#include "../CommandHandler.h"
#include "../../API/defs.h"
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerRefresh : public CommandHandler
    {
    protected:

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            result = (ResultCode)ApiGlobal::sPictureRenderer->Refresh();
            return result;
        }
    };


}

