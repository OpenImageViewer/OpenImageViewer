#include "PreCompiled.h"
#include "CommandProcessor.h"
#include "oiv.h"



namespace OIV
{
    //extern std::unique_ptr<OIV> gViewer;

    IPictureRenderer* CommandProcessor::sPictureRenderer = NULL;

    ResultCode CommandProcessor::ProcessCommand(CommandExecute command, size_t requestSize, void* requestData, size_t responseSize, void* responseData)
    {

        if (command != CE_Init && IsInitialized() == false)
            return ResultCode::RC_NotInitialized;

        ResultCode result = ResultCode::RC_Success;

        switch (command)
        {
        case CE_Init:
            if (sPictureRenderer == NULL)
            {
                sPictureRenderer = new OIV();

                if (requestSize == sizeof(CmdDataInit))
                {
                    // TODO: add width and height.
                    //sPictureRenderer->SetParentParamaters()

                    CmdDataInit* dataZoom = reinterpret_cast<CmdDataInit*>(requestData);
                    sPictureRenderer->SetParent((HWND)dataZoom->parentHandle);
                    sPictureRenderer->Init();
                    Log(_T("Render engine started."));
                }
                else
                    result = RC_WrongDataSize;

            }
            else
                result = ResultCode::RC_AlreadyInitialized;

            break;
        case CE_Zoom:
            if (requestSize == sizeof(CmdDataZoom))
            {
                CmdDataZoom* dataZoom = reinterpret_cast<CmdDataZoom*>(requestData);
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
                CmdDataPan* dataPan = reinterpret_cast<CmdDataPan*>(requestData);
                sPictureRenderer->Pan(dataPan->x, dataPan->y);

            }
            else
            {
                result = RC_WrongDataSize;
            }
            break;

        case CE_LoadFile:
            if (requestSize == sizeof(CmdDataLoadFile))
            {
                const size_t maxPath = 5000;

                CmdDataLoadFile* dataLoadFile = reinterpret_cast<CmdDataLoadFile*>(requestData);

                const size_t bufferLength = dataLoadFile->FileNamelength;

                if (bufferLength <= maxPath)
                {
                    OIVCHAR* buffer = new OIVCHAR[bufferLength + 1];

                    _tcscpy_s(buffer, bufferLength + 1, dataLoadFile->filePath);
                    buffer[bufferLength] = '\0';

                    if (sPictureRenderer->LoadFile(buffer,dataLoadFile->onlyRegisteredExtension) == false)
                        result = RC_UknownError;

                    delete[]buffer;
                }
                else
                {
                    result = RC_InvalidParameters;
                }

                //Optional response
                if (result == RC_Success &&  responseSize == sizeof(CmdResponseLoad))
                {
                    CmdResponseLoad* loadResponse = reinterpret_cast<CmdResponseLoad*>(responseData);
                    Image* image = sPictureRenderer->GetImage();
                    loadResponse->width = image->GetWidth();
                    loadResponse->height = image->GetHeight();
                    loadResponse->bpp = image->GetBitsPerTexel();
                    loadResponse->loadTime = image->GetLoadTime();
                }

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
            if (requestSize == sizeof(CmdRequestFilter))
            {
                CmdRequestFilter* data = reinterpret_cast<CmdRequestFilter*>(requestData);
                
                if (sPictureRenderer->SetFilterLevel(data->filterLevel) != 0)
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
                CmdRequestTexelAtMousePos* request = reinterpret_cast<CmdRequestTexelAtMousePos*>(requestData);
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
                CmdRequestTexelGrid* request = reinterpret_cast<CmdRequestTexelGrid*>(requestData);

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
                CmdSetClientSizeRequest* request = reinterpret_cast<CmdSetClientSizeRequest*>(requestData);
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
        }

        return result;
    }
   
    void CommandProcessor::Log(OIVCHAR * message)
    {
        Ogre::LogManager::getSingleton().logMessage(StringUtility::ToAString(message));
    }
}