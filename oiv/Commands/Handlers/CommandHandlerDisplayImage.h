#pragma once
#pragma once
#include "../CommandHandler.h"
#include <API/defs.h>
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerDisplayImage : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_REQUEST(OIV_CMD_DisplayImage_Request, requestSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ImageHandle handle = ImageNullHandle;
            ResultCode result = RC_UknownError;
            const OIV_CMD_DisplayImage_Request* displayFile = reinterpret_cast<const OIV_CMD_DisplayImage_Request*>(request);

            result =  ApiGlobal::sPictureRenderer->DisplayFile(
                      displayFile->handle
                    , displayFile->displayFlags
                );

            return result;
        }
    };


}

