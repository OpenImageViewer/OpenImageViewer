#pragma once
#pragma once
#include "../CommandHandler.h"
#include "defs.h"
#include "../Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerResampleImage : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY(OIV_CMD_Resample_Request, requestSize, OIV_CMD_Resample_Response, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ImageHandle handle = ImageHandleNull;
            ResultCode result = RC_UknownError;

            const OIV_CMD_Resample_Request* req = reinterpret_cast<const OIV_CMD_Resample_Request*>(request);
            OIV_CMD_Resample_Response* res = reinterpret_cast<OIV_CMD_Resample_Response*>(response);
            result = ApiGlobal::sPictureRenderer->ResampleImage(*req, handle);
            res->imageHandle = handle;

            return result;
        }
    };
}