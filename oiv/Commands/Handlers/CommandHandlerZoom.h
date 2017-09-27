#pragma once
#pragma once
#include "../CommandHandler.h"
#include <API/defs.h>
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerZoom : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(CmdDataZoom, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            const CmdDataZoom* dataZoom = reinterpret_cast<const CmdDataZoom*>(request);
            result = (ResultCode)(int) ApiGlobal::sPictureRenderer->Zoom(dataZoom->amount);

            return result;
        }
    };


}

