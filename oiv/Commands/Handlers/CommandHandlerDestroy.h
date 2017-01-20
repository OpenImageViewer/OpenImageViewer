#pragma once
#pragma once
#include "../CommandHandler.h"
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerDestroy : public CommandHandler
    {
    protected:
        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            CommandProcessor::sPictureRenderer.reset();
            return RC_Success;
        }
    };


}

