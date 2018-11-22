#pragma once
#include "../CommandHandler.h"
#include <API/defs.h>
#include "../CommandProcessor.h"
#include "../../ApiGlobal.h"

namespace OIV
{

    class CommandHandlerQueryImageInfo : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {

            return VERIFY(OIV_CMD_QueryImageInfo_Request, requestSize, OIV_CMD_QueryImageInfo_Response, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            const OIV_CMD_QueryImageInfo_Request* requestT = reinterpret_cast<const OIV_CMD_QueryImageInfo_Request*>(request);
            OIV_CMD_QueryImageInfo_Response* responseT = reinterpret_cast<OIV_CMD_QueryImageInfo_Response*>(response);
            result = ApiGlobal::sPictureRenderer->GetFileInformation(requestT->handle, *responseT);

            return result;
        }
    };


}

