#pragma once
#include "../CommandHandler.h"
#include "../../API/defs.h"
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerColorExposure : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(OIV_CMD_ColorExposure_Request, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            const OIV_CMD_ColorExposure_Request* exposure = reinterpret_cast<const OIV_CMD_ColorExposure_Request*>(request);
            result = ApiGlobal::sPictureRenderer->SetColorExposure(*exposure);

            return result;
        }
    };


}

