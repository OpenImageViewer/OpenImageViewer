#pragma once
#pragma once
#include "../CommandHandler.h"
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerTexelAtMousePos : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY(CmdRequestTexelAtMousePos, requestSize, CmdResponseTexelAtMousePos , responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            
            const CmdRequestTexelAtMousePos* req = reinterpret_cast<const CmdRequestTexelAtMousePos*>(request);
            CmdResponseTexelAtMousePos* res = reinterpret_cast<CmdResponseTexelAtMousePos*>(response);

            result = (ResultCode)CommandProcessor::sPictureRenderer->GetTexelAtMousePos(req->x, req->y, res->x, res->y);

            return result;
        }
    };
}

