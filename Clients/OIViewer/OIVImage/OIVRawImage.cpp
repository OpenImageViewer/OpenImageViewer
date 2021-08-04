#include "OIVRawImage.h"
#include "../OIVCommands.h"

namespace OIV
{
    ResultCode OIVRawImage::Load(const RawBufferParams & loadParams)
    {

        using namespace LLUtils;

        OIV_CMD_LoadRaw_Response loadResponse;
        OIV_CMD_LoadRaw_Request loadRequest = {};
        loadRequest.buffer = const_cast<std::byte*>(loadParams.buffer);
        loadRequest.width = loadParams.width;
        loadRequest.height = loadParams.height;
        loadRequest.rowPitch = loadParams.rowPitch;
        loadRequest.texelFormat = loadParams.texelFormat;
        loadRequest.transformation = OIV_AxisAlignedFlip::AAF_Vertical;


        ResultCode result = OIVCommands::ExecuteCommand(CommandExecute::OIV_CMD_LoadRaw, &loadRequest, &loadResponse);
        if (result == RC_Success)
        {
            ImageDescriptor& desc = GetDescriptorMutable();
            OIV_Util_GetBPPFromTexelFormat(loadParams.texelFormat, &desc.Bpp);
            desc.Height = loadParams.height;
            desc.Width = loadParams.width;
            SetImageHandle(loadResponse.handle);
            desc.Source = fImageSource;
            desc.LoadTime = loadResponse.loadTime;;
            desc.texelFormat = loadParams.texelFormat;
        }

        return result;

    }
}