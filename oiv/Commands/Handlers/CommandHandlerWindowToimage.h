#pragma once
#pragma once
#pragma once
#include "../CommandHandler.h"
#include <API/defs.h>
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerWindowToImage : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY(OIV_CMD_WindowToImage_Request, requestSize, OIV_CMD_WindowToImage_Response, requestSize);

        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ImageHandle handle = ImageNullHandle;
            ResultCode result = RC_UknownError;

            const OIV_CMD_WindowToImage_Request* req = reinterpret_cast<const OIV_CMD_WindowToImage_Request*>(request);
            OIV_CMD_WindowToImage_Response* res = reinterpret_cast<OIV_CMD_WindowToImage_Response*>(response);

            return ApiGlobal::sPictureRenderer->WindowToImage(*req, *res);
        }
    };


}

