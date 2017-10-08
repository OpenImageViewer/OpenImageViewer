#pragma once
#include "../CommandHandler.h"
#include <API/defs.h>
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerPan : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(CmdDataPan, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;
            const CmdDataPan* dataPan = reinterpret_cast<const CmdDataPan*>(request);
            result = (ResultCode)ApiGlobal::sPictureRenderer->SetOffset(dataPan->x, dataPan->y);

            return result;
        }
    };


}

