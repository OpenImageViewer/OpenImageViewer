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
        fCommandHandlers.emplace(CE_Init, std::make_unique<CommandHandlerInit>());
        fCommandHandlers.emplace(OIV_CMD_LoadFile, std::make_unique<CommandHandlerLoadFile>());
        fCommandHandlers.emplace(OIV_CMD_LoadRaw, std::make_unique<CommandHandlerLoadRaw>());
        fCommandHandlers.emplace(OIV_CMD_UnloadFile, std::make_unique<CommandHandlerUnloadFile>());
        fCommandHandlers.emplace(CE_Refresh, std::make_unique<CommandHandlerRefresh>());
        fCommandHandlers.emplace(CE_TexelGrid, std::make_unique<CommandHandlerTexelGrid>());
        fCommandHandlers.emplace(CMD_SetClientSize, std::make_unique<CommandHandlerSetClientSize>());
        fCommandHandlers.emplace(OIV_CMD_Destroy, std::make_unique<CommandHandlerDestroy>());
        fCommandHandlers.emplace(OIV_CMD_AxisAlignedTransform, std::make_unique<CommandHandlerAxisAlignedTransform>());
        fCommandHandlers.emplace(OIV_CMD_ImageProperties, std::make_unique<CommandHandlerImageProperties>());
        fCommandHandlers.emplace(OIV_CMD_SetSelectionRect, std::make_unique<CommandHandlerSetSelectionRect>());
        fCommandHandlers.emplace(OIV_CMD_CropImage, std::make_unique<CommandHandlerCropImage>());
        fCommandHandlers.emplace(OIV_CMD_GetPixels, std::make_unique<CommandHandlerGetPixels>());
        fCommandHandlers.emplace(OIV_CMD_ConvertFormat, std::make_unique<CommandHandlerConvertFormat>());
        fCommandHandlers.emplace(OIV_CMD_ColorExposure, std::make_unique<CommandHandlerColorExposure>());
        fCommandHandlers.emplace(OIV_CMD_TexelInfo, std::make_unique<CommandHandlerTexelInfo>());
        fCommandHandlers.emplace(OIV_CMD_QueryImageInfo, std::make_unique<CommandHandlerQueryImageInfo>());
        fCommandHandlers.emplace(OIV_CMD_CreateText, std::make_unique<CommandHandlerCreateText>());
        fCommandHandlers.emplace(OIV_CMD_GetKnownFileTypes, std::make_unique<CommandHandlerGetKnownFileTypes>());
        fCommandHandlers.emplace(OIV_CMD_RegisterCallbacks, std::make_unique<CommandHandlerRegisterCallbacks>());
        fCommandHandlers.emplace(OIV_CMD_GetSubImages, std::make_unique<CommandHandlerGetSubImages>());
        fCommandHandlers.emplace(OIV_CMD_ResampleImage, std::make_unique<CommandHandlerResampleImage>());
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
