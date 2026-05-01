#pragma once
#include "../CommandHandler.h"
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerTexelGrid : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST (CmdRequestTexelGrid, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_Success;

            const CmdRequestTexelGrid* req= reinterpret_cast<const CmdRequestTexelGrid*>(request);

            result = (ResultCode)ApiGlobal::sPictureRenderer->SetTexelGrid(*req);
            
            return result;
        }
    };


}


