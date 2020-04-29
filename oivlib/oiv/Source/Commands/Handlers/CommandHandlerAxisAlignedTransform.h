#pragma once
#include "../CommandHandler.h"
#include <defs.h>
#include "../CommandProcessor.h"

namespace OIV
{

    class CommandHandlerAxisAlignedTransform : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY(OIV_CMD_AxisAlignedTransform_Request, requestSize, OIV_CMD_AxisAlignedTransform_Response, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ImageHandle handle = ImageHandleNull;
            ResultCode result = RC_UknownError;

            const OIV_CMD_AxisAlignedTransform_Request* req = reinterpret_cast<const OIV_CMD_AxisAlignedTransform_Request*>(request);
            OIV_CMD_AxisAlignedTransform_Response* res = reinterpret_cast<OIV_CMD_AxisAlignedTransform_Response*>(response);
            result = ApiGlobal::sPictureRenderer->AxisAlignTrasnform(*req, *res);

          

            return result;
        }
    };
}