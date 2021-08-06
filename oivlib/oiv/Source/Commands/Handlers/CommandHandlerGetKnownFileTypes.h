#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerGetKnownFileTypes : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_RESPONSE(OIV_CMD_GetKnownFileTypes_Response, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            OIV_CMD_GetKnownFileTypes_Response* res = reinterpret_cast<OIV_CMD_GetKnownFileTypes_Response*>(response);
            return ApiGlobal::sPictureRenderer->GetKnownFileTypes(*res);
        }
    };


}


