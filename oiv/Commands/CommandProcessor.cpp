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
    CommandProcessor::CommandProcessor()
    {
        using namespace std;
        fCommandHandlers =
        {
              make_pair(CE_Init,new CommandHandlerInit())
            , make_pair(OIV_CMD_LoadFile,new CommandHandlerLoadFile())
            , make_pair(OIV_CMD_UnloadFile,new CommandHandlerUnloadFile())
            , make_pair(OIV_CMD_DisplayImage,new CommandHandlerDisplayImage())
            , make_pair(CE_Zoom,new CommandHandlerZoom())
            , make_pair(CE_Pan,new CommandHandlerPan())
            , make_pair(CE_Refresh,new CommandHandlerRefresh())
            , make_pair(CE_TexelAtMousePos,new CommandHandlerTexelAtMousePos())
            , make_pair(CE_TexelGrid,new CommandHandlerTexelGrid())
            , make_pair(CMD_GetNumTexelsInCanvas,new CommandHandlerNumTexelsInCanvas())
            , make_pair(CMD_SetClientSize,new CommandHandlerSetClientSize())
            , make_pair(OIV_CMD_Destroy,new CommandHandlerDestroy())
            , make_pair(OIV_CMD_AxisAlignedTransform,new CommandHandlerAxisAlignedTransform())
            , make_pair(CE_FilterLevel,new CommandHandlerFilterLevel())
            , make_pair(OIV_CMD_ZoomScrollState,new CommandHandlerZoomScrollState())
        };
    }

    CommandProcessor::~CommandProcessor()
    {
        for (auto &pair : fCommandHandlers)
            delete pair.second;
    }
 

    ResultCode CommandProcessor::ProcessCommand(CommandExecute command, const std::size_t requestSize, const void* requestData, const std::size_t responseSize, void* responseData)
    {
        auto pair = fCommandHandlers.find(command);
        return pair != fCommandHandlers.end() ? pair->second->Execute(requestData, requestSize, responseData, responseSize) : RC_UnknownCommand;
    }

   /* bool CommandProcessor::IsInitialized() const
    {
        return fPictureRenderer != nullptr;
    }*/

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
