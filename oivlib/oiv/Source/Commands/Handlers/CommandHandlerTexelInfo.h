#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"
#include "../../ApiGlobal.h"

namespace OIV
{
    class CommandHandlerTexelInfo : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY(OIV_CMD_TexelInfo_Request, requestSize, OIV_CMD_TexelInfo_Response, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            const OIV_CMD_TexelInfo_Request* texelRequest = reinterpret_cast<const OIV_CMD_TexelInfo_Request*>(request);
            OIV_CMD_TexelInfo_Response* texelresponse = reinterpret_cast<OIV_CMD_TexelInfo_Response*>(response);
            result = ApiGlobal::sPictureRenderer->GetTexelInfo(*texelRequest,*texelresponse);

            return result;
        }
    };
}