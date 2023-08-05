#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerImageProperties : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(OIV_CMD_ImageProperties_Request, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            //const OIV_CMD_ImageProperties_Request* properties = reinterpret_cast<const OIV_CMD_ImageProperties_Request*>(request);
            //result = (ResultCode)ApiGlobal::sPictureRenderer->SetImageProperties(*properties);

            return result;
        }
    };


}

