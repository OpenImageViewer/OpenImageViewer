#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"
#include "../../ApiGlobal.h"

namespace OIV
{

    class CommandHandlerLoadRaw : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_OPTIONAL_RESPONSE(OIV_CMD_LoadRaw_Request, requestSize, OIV_CMD_LoadRaw_Response, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ImageHandle handle = ImageHandleNull;
            ResultCode result = RC_UknownError;
            const OIV_CMD_LoadRaw_Request* dataLoadRaw = (reinterpret_cast<const OIV_CMD_LoadRaw_Request*>(request));

            result = static_cast<ResultCode>(
                ApiGlobal::sPictureRenderer->LoadRaw(*dataLoadRaw, handle));



            if (result == RC_Success &&  responseSize > 1)
            {
                OIV_CMD_LoadRaw_Response* loadResponse = reinterpret_cast<OIV_CMD_LoadRaw_Response*>(response);
                IMCodec::ImageSharedPtr image = ApiGlobal::sPictureRenderer->GetImage(handle);
                loadResponse->loadTime = image->GetProcessData().processTime;// GetRuntimeData().loadTime;
                loadResponse->handle = handle;
            }

            return result;
        }
    };


}