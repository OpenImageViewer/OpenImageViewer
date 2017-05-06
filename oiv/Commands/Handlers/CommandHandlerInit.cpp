#include "CommandHandlerInit.h"
#include "Commands/CommandProcessor.h"
#include "oiv.h"
#include "ApiGlobal.h"


namespace OIV
{
    ResultCode CommandHandlerInit::ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize)
    {

        ResultCode result = RC_UknownError;
        //if (ApiGlobal::sPictureRenderer == nullptr)
        {
            //ApiGlobal::sPictureRenderer = std::unique_ptr<IPictureRenderer>(new OIV());
            // TODO: add width and height.
            //sPictureRenderer->SetParentParamaters()

            const CmdDataInit* dataInit = reinterpret_cast<const CmdDataInit*>(request);
            ApiGlobal::sPictureRenderer->SetParent(static_cast<std::size_t>(dataInit->parentHandle));
            ApiGlobal::sPictureRenderer->Init();
            result = RC_Success;

        }
        //else
          //  result = ResultCode::RC_AlreadyInitialized;

        return result;
    }

}
