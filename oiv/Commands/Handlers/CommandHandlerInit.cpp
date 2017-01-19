#include "CommandHandlerInit.h"
#include "Commands/CommandProcessor.h"
#include "oiv.h"


namespace OIV
{
    ResultCode CommandHandlerInit::ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize)
    {

        ResultCode result = RC_UknownError;
        if (CommandProcessor::sPictureRenderer == nullptr)
        {
            CommandProcessor::sPictureRenderer = std::unique_ptr<IPictureRenderer>(new OIV());
            // TODO: add width and height.
            //sPictureRenderer->SetParentParamaters()

            const CmdDataInit* dataInit = reinterpret_cast<const CmdDataInit*>(request);
            CommandProcessor::sPictureRenderer->SetParent(static_cast<std::size_t>(dataInit->parentHandle));
            CommandProcessor::sPictureRenderer->Init();
            result = RC_Success;

        }
        else
            result = ResultCode::RC_AlreadyInitialized;

        return result;
    }

}
