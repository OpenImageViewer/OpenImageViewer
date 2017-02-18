#include "CommandProcessor.h"
#include <memory>
#include "Handlers/CommandHandlerInit.h"
#include "Handlers/CommandHandlerLoadFile.h"
#include "Handlers/CommandHandlerDisplayImage.h"
#include "Handlers/CommandHandlerPan.h"
#include "Handlers/CommandHandlerZoom.h"
#include "Handlers/CommandHandlerRefresh.h"
#include "Handlers/CommandHandlerTexelAtMousePos.h"
#include "Handlers/CommandHandlerTexelGrid.h"
#include "Handlers/CommandHandlerFilterLevel.h"
#include "Handlers/CommandHandlerNumTexelsInCanvas.h"
#include "Handlers/CommandHandlerSetClientSize.h"
#include "Handlers/CommandHandlerDestroy.h"
#include "Handlers/CommandHandlerAxisAlignedTransform.h"
#include "Handlers/CommandHandlerUnloadFile.h"
#include "oiv.h"
#include "Handlers/CommandHandlerZoomScrollState.h"


namespace OIV
{
    std::unique_ptr<IPictureRenderer> CommandProcessor::sPictureRenderer = std::unique_ptr<IPictureRenderer>(new OIV());
    CommandProcessor::MapCommanderHandler CommandProcessor::sCommandHandlers =

    {
          std::make_pair(CE_Init,new CommandHandlerInit())
        , std::make_pair(OIV_CMD_LoadFile,new CommandHandlerLoadFile())
        , std::make_pair(OIV_CMD_UnloadFile,new CommandHandlerUnloadFile())
        , std::make_pair(OIV_CMD_DisplayImage,new CommandHandlerDisplayImage())
        , std::make_pair(CE_Zoom,new CommandHandlerZoom())
        , std::make_pair(CE_Pan,new CommandHandlerPan())
        , std::make_pair(CE_Refresh,new CommandHandlerRefresh())
        , std::make_pair(CE_TexelAtMousePos,new CommandHandlerTexelAtMousePos())
        , std::make_pair(CE_TexelGrid,new CommandHandlerTexelGrid())
        , std::make_pair(CMD_GetNumTexelsInCanvas,new CommandHandlerNumTexelsInCanvas())
        , std::make_pair(CMD_SetClientSize,new CommandHandlerSetClientSize())
        , std::make_pair(OIV_CMD_Destroy,new CommandHandlerDestroy())
        , std::make_pair(OIV_CMD_AxisAlignedTransform,new CommandHandlerAxisAlignedTransform())
        , std::make_pair(CE_FilterLevel,new CommandHandlerFilterLevel())
        , std::make_pair(OIV_CMD_ZoomScrollState,new CommandHandlerZoomScrollState())
    };

	ResultCode CommandProcessor::ProcessCommand(CommandExecute command, const std::size_t requestSize, const void* requestData, const std::size_t responseSize, void* responseData)
    {
        auto pair = sCommandHandlers.find(command);
        return pair != sCommandHandlers.end() ? pair->second->Execute(requestData, requestSize, responseData, responseSize) : RC_UnknownCommand;
    }


    /*  case CE_GetFileInformation:

    if (responseSize == sizeof(QryFileInformation))
    {
    QryFileInformation fileInfo;
    memset(&fileInfo, 0, sizeof(fileInfo));

    if (sPictureRenderer->GetFileInformation(fileInfo) != 0)
    result = ResultCode::RC_WrongDataSize;
    else
    {
    QryFileInformation* data = reinterpret_cast<QryFileInformation*>(responseData);
    *reinterpret_cast<QryFileInformation*>(responseData) = fileInfo;
    }
    }
    break;*/

}
