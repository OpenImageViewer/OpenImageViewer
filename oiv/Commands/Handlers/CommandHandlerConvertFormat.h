#pragma once
#pragma once
#include "../CommandHandler.h"
#include <API/defs.h>
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerConvertFormat : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY(OIV_CMD_ConvertFormat_Request, requestSize, OIV_CMD_ConvertFormat_Response, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            const OIV_CMD_ConvertFormat_Request* req = reinterpret_cast<const OIV_CMD_ConvertFormat_Request*>(request);
            OIV_CMD_ConvertFormat_Response* res = reinterpret_cast<OIV_CMD_ConvertFormat_Response*>(response);
            return ApiGlobal::sPictureRenderer->ConverFormat(*req,*res);
        }
    };


}

