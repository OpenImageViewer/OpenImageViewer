#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerGetSubImages : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY(OIV_CMD_GetSubImages_Request, requestSize, OIV_CMD_GetSubImages_Response,responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_UknownError;
            const OIV_CMD_GetSubImages_Request* req = reinterpret_cast<const OIV_CMD_GetSubImages_Request*>(request);
            OIV_CMD_GetSubImages_Response* res = reinterpret_cast<OIV_CMD_GetSubImages_Response*>(response);
            result = ApiGlobal::sPictureRenderer->GetSubImages(*req,*res);
            return result;
        }
    };
}
