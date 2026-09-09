#include "CommandHandlerInit.h"
#include "../CommandProcessor.h"
#include "../../ApiGlobal.h"

namespace OIV
{
    ResultCode CommandHandlerInit::ExecuteImpl(const void* request, [[maybe_unused]] const std::size_t requestSize,
                                               [[maybe_unused]] void* response,
                                               [[maybe_unused]] const std::size_t responseSize)
    {
        const auto* dataInit = static_cast<const CmdDataInit*>(request);
        try
        {
            ApiGlobal::sPictureRenderer->SetParent(dataInit->parentHandle, dataInit->nativeDisplay);
            if (dataInit->rendering != nullptr)
                ApiGlobal::sPictureRenderer->Init(*dataInit->rendering);
            else
                ApiGlobal::sPictureRenderer->Init();
        }
        catch (...)
        {
            if (dataInit->initializationError == nullptr)
                throw;
            *dataInit->initializationError = std::current_exception();
            return RC_RenderError;
        }
        return RC_Success;
    }

}  // namespace OIV
