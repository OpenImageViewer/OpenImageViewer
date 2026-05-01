#pragma once
#include "../CommandHandler.h"
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerDestroy : public CommandHandler
    {
    protected:
        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ApiGlobal::sPictureRenderer.reset();
            return RC_Success;
        }
    };


}

