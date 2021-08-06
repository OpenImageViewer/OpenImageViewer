#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerSetSelectionRect : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(OIV_CMD_SetSelectionRect_Request, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_UknownError;
            const OIV_CMD_SetSelectionRect_Request* requestData = reinterpret_cast<const OIV_CMD_SetSelectionRect_Request*>(request);

            result = ApiGlobal::sPictureRenderer->SetSelectionRect(*requestData);

            return result;
        }
    };


}

