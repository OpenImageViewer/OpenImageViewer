#include "APIImpl.h"
#include "Commands/CommandProcessor.h"
#include "ApiGlobal.h"

namespace OIV
{

    ResultCode Execute_impl(int command, std::size_t requestSize, void* requestData, std::size_t responseSize, void* responseData)
    {
        return ApiGlobal::sCommandProcessor.ProcessCommand(static_cast<CommandExecute>(command), requestSize, requestData, responseSize, responseData);
    }

    namespace Util
    {
        
        ResultCode GetBPPFromTexelFormat_impl(OIV_TexelFormat in_texelFormat, uint8_t* out_bpp)
        {
            *out_bpp = IMCodec::GetTexelFormatSize(static_cast<IMCodec::TexelFormat>(in_texelFormat));
            return RC_Success;
        }
    }
}

