#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerCreateText : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY(OIV_CMD_CreateText_Request, requestSize, OIV_CMD_CreateText_Response, responseSize);

        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {

            const OIV_CMD_CreateText_Request* req = reinterpret_cast<const OIV_CMD_CreateText_Request*>(request);
            OIV_CMD_CreateText_Response* res = reinterpret_cast<OIV_CMD_CreateText_Response*>(response);

            return ApiGlobal::sPictureRenderer->CreateText(*req, *res);
        }
    };


}

