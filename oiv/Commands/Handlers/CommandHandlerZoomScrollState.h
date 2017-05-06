#pragma once
#pragma once
#pragma once
#include "../CommandHandler.h"
#include <API/defs.h>
#include "ApiGlobal.h"

namespace OIV
{

    class CommandHandlerZoomScrollState : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(OIV_CMD_ZoomScrollState_Request, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            const OIV_CMD_ZoomScrollState_Request* zoomScrollState = reinterpret_cast<const OIV_CMD_ZoomScrollState_Request*>(request);
            result = ApiGlobal::sPictureRenderer->SetZoomScrollState(zoomScrollState);

            return result;
        }
    };


}

