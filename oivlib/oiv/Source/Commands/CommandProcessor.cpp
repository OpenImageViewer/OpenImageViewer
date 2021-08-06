#include <LLUtils/Warnings.h>
LLUTILS_DISABLE_WARNING_PUSH
LLUTILS_DISABLE_WARNING_UNUSED_PARAMETER
#include "CommandProcessor.h"
#include "Handlers/CommandHandlerInit.h"
#include "Handlers/CommandHandlerLoadFile.h"
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
#include "Handlers/CommandHandlerRegisterCallbacks.h"
#include "Handlers/CommandHandlerGetSubImages.h"
#include "Handlers/CommandHandlerResampleImage.h"
LLUTILS_DISABLE_WARNING_POP

namespace OIV
{
    CommandProcessor::CommandProcessor()
    {
        using namespace std;
        fCommandHandlers =
        {
              {CE_Init,new CommandHandlerInit()}
            , {OIV_CMD_LoadFile,new CommandHandlerLoadFile()}
            , {OIV_CMD_LoadRaw,new CommandHandlerLoadRaw()}
            , {OIV_CMD_UnloadFile,new CommandHandlerUnloadFile()}
            , {CE_Refresh,new CommandHandlerRefresh()}
            , {CE_TexelGrid,new CommandHandlerTexelGrid()}
            , {CMD_SetClientSize,new CommandHandlerSetClientSize()}
            , {OIV_CMD_Destroy,new CommandHandlerDestroy()}
            , {OIV_CMD_AxisAlignedTransform,new CommandHandlerAxisAlignedTransform()}
            , {OIV_CMD_ImageProperties,new CommandHandlerImageProperties()}
            , {OIV_CMD_SetSelectionRect,new CommandHandlerSetSelectionRect()}
            , {OIV_CMD_CropImage,new CommandHandlerCropImage()}
            , {OIV_CMD_GetPixels,new CommandHandlerGetPixels()}
            , {OIV_CMD_ConvertFormat,new CommandHandlerConvertFormat()}
            , {OIV_CMD_ColorExposure,new CommandHandlerColorExposure()}
            , {OIV_CMD_TexelInfo,new CommandHandlerTexelInfo()}
            , {OIV_CMD_QueryImageInfo,new CommandHandlerQueryImageInfo()}
            , {OIV_CMD_CreateText,new CommandHandlerCreateText()}
            , {OIV_CMD_GetKnownFileTypes,new CommandHandlerGetKnownFileTypes()}
            , {OIV_CMD_RegisterCallbacks,new CommandHandlerRegisterCallbacks()}
            , {OIV_CMD_GetSubImages,new CommandHandlerGetSubImages()}
            , {OIV_CMD_ResampleImage,new CommandHandlerResampleImage()}
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
        if (pair != fCommandHandlers.end())
        {
            try
            {
                return pair->second->Execute(requestData, requestSize, responseData, responseSize);
            }

            catch (...)
            {
                return RC_InternalError;
            }
        }
        else
        {
            return RC_UnknownCommand;
        }
    }
}
