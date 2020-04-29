#pragma once
#include "../CommandHandler.h"
#include <defs.h>

namespace OIV
{

    class CommandHandlerInit : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(CmdDataInit, requestSize);
        }

        // Implementation at cpp file
        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override;
    };

  
}
