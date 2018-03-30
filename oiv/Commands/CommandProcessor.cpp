#include "CommandProcessor.h"
#include "Handlers/CommandHandlerInit.h"
#include "Handlers/CommandHandlerLoadFile.h"
#include "Handlers/CommandHandlerDisplayImage.h"
#include "Handlers/CommandHandlerRefresh.h"
#include "Handlers/CommandHandlerTexelGrid.h"
#include "Handlers/CommandHandlerSetClientSize.h"
#include "Handlers/CommandHandlerDestroy.h"
#include "Handlers/CommandHandlerAxisAlignedTransform.h"
#include "Handlers/CommandHandlerUnloadFile.h"
#include "Handlers/CommandHandlerLoadRaw.h"
#include "Handlers/CommandHandlerSetSelectionRect.h"
#include "Handlers/CommandHandlerCropImage.h"
#include "Handlers/CommandHandlerGetPixels.h"
#include "Handlers/CommandHandlerConvertFormat.h"
#include "Handlers/CommandHandlerColorExposure.h"
#include "Handlers/CommandHandlerTexelInfo.h"
#include "Handlers/CommandHandlerQueryImageInfo.h"
#include "Handlers/CommandHandlerImageProperties.h"
#include "Handlers/CommandHandlerCreateText.h"
#include "Handlers/CommandHandlerGetKnownFileTypes.h"


namespace OIV
{
    CommandProcessor::CommandProcessor()
    {
        using namespace std;
        fCommandHandlers =
        {
              make_pair(CE_Init,new CommandHandlerInit())
            , make_pair(OIV_CMD_LoadFile,new CommandHandlerLoadFile())
            , make_pair(OIV_CMD_LoadRaw,new CommandHandlerLoadRaw())
            , make_pair(OIV_CMD_UnloadFile,new CommandHandlerUnloadFile())
            , make_pair(OIV_CMD_DisplayImage,new CommandHandlerDisplayImage())
            , make_pair(CE_Refresh,new CommandHandlerRefresh())
            , make_pair(CE_TexelGrid,new CommandHandlerTexelGrid())
            , make_pair(CMD_SetClientSize,new CommandHandlerSetClientSize())
            , make_pair(OIV_CMD_Destroy,new CommandHandlerDestroy())
            , make_pair(OIV_CMD_AxisAlignedTransform,new CommandHandlerAxisAlignedTransform())
            , make_pair(OIV_CMD_ImageProperties,new CommandHandlerImageProperties())
            , make_pair(OIV_CMD_SetSelectionRect,new CommandHandlerSetSelectionRect())
            , make_pair(OIV_CMD_CropImage,new CommandHandlerCropImage())
            , make_pair(OIV_CMD_GetPixels,new CommandHandlerGetPixels())
            , make_pair(OIV_CMD_ConvertFormat,new CommandHandlerConvertFormat())
            , make_pair(OIV_CMD_ColorExposure,new CommandHandlerColorExposure())
            , make_pair(OIV_CMD_TexelInfo,new CommandHandlerTexelInfo())
            , make_pair(OIV_CMD_QueryImageInfo,new CommandHandlerQueryImageInfo())
            , make_pair(OIV_CMD_CreateText,new CommandHandlerCreateText())
            , make_pair(OIV_CMD_GetKnownFileTypes,new CommandHandlerGetKnownFileTypes())
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
}
