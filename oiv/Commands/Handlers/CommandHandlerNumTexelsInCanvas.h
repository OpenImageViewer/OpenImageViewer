#pragma once
#pragma once
#include "../CommandHandler.h"
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerNumTexelsInCanvas : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_RESPONSE(CmdGetNumTexelsInCanvasResponse, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            CmdGetNumTexelsInCanvasResponse* res = reinterpret_cast<CmdGetNumTexelsInCanvasResponse*>(response);
            result = (ResultCode)ApiGlobal::sPictureRenderer->GetNumTexelsInCanvas(res->width, res->height);
            return result;
        }
    };
}

