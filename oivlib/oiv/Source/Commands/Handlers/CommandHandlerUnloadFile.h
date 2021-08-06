#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerUnloadFile : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(OIV_CMD_UnloadFile_Request, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_UknownError;
            const OIV_CMD_UnloadFile_Request* req = reinterpret_cast<const OIV_CMD_UnloadFile_Request*>(request);


            result = static_cast<ResultCode>(
                ApiGlobal::sPictureRenderer->UnloadFile(
                    req->handle
                ));

            return result;
        }
    };
}
