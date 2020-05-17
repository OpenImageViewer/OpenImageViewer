#pragma once
#include "../CommandHandler.h"
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerSetClientSize : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(CmdSetClientSizeRequest, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            const CmdSetClientSizeRequest* req = reinterpret_cast<const CmdSetClientSizeRequest*>(request);
            if (ApiGlobal::sPictureRenderer->SetClientSize(req->width, req->height))
            {
                result = RC_UknownError;
            }
            return result;
        }
    };
}

