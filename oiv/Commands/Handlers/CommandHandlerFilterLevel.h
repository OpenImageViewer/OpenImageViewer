#pragma once
#pragma once
#include "../CommandHandler.h"
#include <API/defs.h>
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerFilterLevel : public CommandHandler
    {
    protected:
        
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(OIV_CMD_Filter_Request, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;

            const OIV_CMD_Filter_Request* req = reinterpret_cast<const OIV_CMD_Filter_Request*>(request);
            result = (ResultCode)CommandProcessor::sPictureRenderer->SetFilterLevel(req->filterType);
            

            return result;
        }
    };


}


