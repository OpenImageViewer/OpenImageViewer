#include "CommandProcessor.h"
#include <memory>
#include "Handlers/CommandHandlerInit.h"
#include "Handlers/CommandHandlerLoadFile.h"


namespace OIV
{
    std::unique_ptr<IPictureRenderer> CommandProcessor::sPictureRenderer;
    CommandProcessor::MapCommanderHandler CommandProcessor::sCommandHandlers = 

    { 
          std::make_pair(CE_Init,new CommandHandlerInit()) 
        , std::make_pair(CE_LoadFile,new CommandHandlerLoadFile())
    };
    

	ResultCode CommandProcessor::ProcessCommand(CommandExecute command, const std::size_t requestSize, const void* requestData, const std::size_t responseSize, void* responseData)
    {
        auto pair = sCommandHandlers.find(command);
        //return pair == sCommandHandlers.end() ? RC_UnknownCommand : pair->second->Execute(requestData, requestSize, responseData, responseSize);

        if (pair != sCommandHandlers.end())
            return pair->second->Execute(requestData, requestSize, responseData, responseSize);

        
        /*if (command != CE_Init && IsInitialized() == false)
            return ResultCode::RC_NotInitialized;*/
        ResultCode result = RC_Success;
     
        switch (command)
        {
          
            break;
        case CE_Zoom:
            if (requestSize == sizeof(CmdDataZoom))
            {
				const CmdDataZoom* dataZoom = reinterpret_cast<const CmdDataZoom*>(requestData);
                sPictureRenderer->Zoom(dataZoom->amount, dataZoom->zoomX, dataZoom->zoomY);

            }
            else
            {
                result = RC_WrongDataSize;
            }
            break;
        case CE_Pan:
            if (requestSize == sizeof(CmdDataPan))
            {
				const CmdDataPan* dataPan = reinterpret_cast<const CmdDataPan*>(requestData);
                sPictureRenderer->Pan(dataPan->x, dataPan->y);

            }
            else
            {
                result = RC_WrongDataSize;
            }
            break;

        case CE_Refresh:
            sPictureRenderer->Refresh();
            break;

        case CE_FilterLevel:
            if (requestSize == sizeof(OIV_CMD_Filter_Request))
            {
				const OIV_CMD_Filter_Request* data = reinterpret_cast<const OIV_CMD_Filter_Request*>(requestData);
                
                if (sPictureRenderer->SetFilterLevel(data->filterType) != 0)
                {
                    result = RC_UknownError;
                }
                    
                
            }
            else
            {
                result = ResultCode::RC_WrongDataSize;
            }
            break;
            

        case CE_GetFileInformation:

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
            break;
        case CE_TexelAtMousePos:
            if (requestSize == sizeof(CmdRequestTexelAtMousePos) && responseSize == sizeof(CmdResponseTexelAtMousePos))
            {
				const CmdRequestTexelAtMousePos* request = reinterpret_cast<const CmdRequestTexelAtMousePos*>(requestData);
                CmdResponseTexelAtMousePos* response = reinterpret_cast<CmdResponseTexelAtMousePos*>(responseData);

                if (sPictureRenderer->GetTexelAtMousePos(request->x,request->y, response->x, response->y) != 0)
                {
                    result = RC_UknownError;
                }
            }
            else
            {
                result = ResultCode::RC_WrongDataSize;
            }
            break;
        case CE_TexelGrid:
            if (requestSize == sizeof(CmdRequestTexelGrid))
            {
				const CmdRequestTexelGrid* request = reinterpret_cast<const CmdRequestTexelGrid*>(requestData);

                if (sPictureRenderer->SetTexelGrid(request->gridSize) != 0)
                {
                    result = RC_UknownError;
                }
            }
            else
            {
                result = ResultCode::RC_WrongDataSize;
            }
            break;
        case CMD_GetNumTexelsInCanvas:
            if (responseSize == sizeof(CmdGetNumTexelsInCanvasResponse))
            {
                CmdGetNumTexelsInCanvasResponse* response = reinterpret_cast<CmdGetNumTexelsInCanvasResponse*>(responseData);
                if (sPictureRenderer->GetNumTexelsInCanvas(response->width, response->height))
                {
                    result = RC_UknownError;
                }
            }
            else
            {
                result = ResultCode::RC_WrongDataSize;
            }

            break;
            
             case CMD_SetClientSize:
            
            if (requestSize == sizeof(CmdSetClientSizeRequest))
            {
				const CmdSetClientSizeRequest* request = reinterpret_cast<const CmdSetClientSizeRequest*>(requestData);
                if (sPictureRenderer->SetClientSize(request->width, request->height))
                {
                    result = RC_UknownError;
                }
            }
            else
            {
                result = ResultCode::RC_WrongDataSize;
            }

            break;

             case OIV_CMD_Destroy:
                 sPictureRenderer.reset();
                 break;
             
             case OIV_CMD_AxisAlignedTransform:
                 const OIV_CMDAxisalignedTransformRequest* request = reinterpret_cast<const OIV_CMDAxisalignedTransformRequest*>(requestData);
                 result = sPictureRenderer->AxisAlignTrasnform(request->transform);
 
                break;
        }

        return result;
    }
}
