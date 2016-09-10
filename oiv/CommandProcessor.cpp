#include "CommandProcessor.h"
#include <memory>
#include "oiv.h"
#include "StringUtility.h"


namespace OIV
{
    //extern std::unique_ptr<OIV> gViewer;
    
    IPictureRenderer* CommandProcessor::sPictureRenderer = NULL;

   ResultCode CommandProcessor::ProcessCommand(CommandExecute command, size_t commandSize, void* commandData)
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

                     if (commandSize == sizeof(CmdDataInit))
                     {
                         // TODO: add width and height.
                         //sPictureRenderer->SetParentParamaters()
                         
                         CmdDataInit* dataZoom = reinterpret_cast<CmdDataInit*>(commandData);
                         sPictureRenderer->SetParent((HWND)dataZoom->parentHandle);
                         sPictureRenderer->Init();
                         Log(_T("Render engine started."));
                     }
                     else
                         result = RC_WrongDataSize;
                    
                }

                break;
        case CE_Zoom:
            if (commandSize == sizeof(CmdDataZoom))
            {
                CmdDataZoom* dataZoom = reinterpret_cast<CmdDataZoom*>(commandData);
                if (sPictureRenderer != NULL)
                    sPictureRenderer->Zoom(dataZoom->amount);

            }
            else
            {
                result = RC_WrongDataSize;
            }
            break;

        case CE_LoadFile:
            if (commandSize == sizeof(CmdDataLoadFile))
            {
                const size_t maxPath = 5000;

                CmdDataLoadFile* dataLoadFile = reinterpret_cast<CmdDataLoadFile*>(commandData);

                const size_t bufferLength = dataLoadFile->FileNamelength;

                if (bufferLength <= maxPath)
                {
                    OIVCHAR* buffer = new OIVCHAR[bufferLength + 1];

                    _tcscpy_s(buffer, bufferLength + 1, dataLoadFile->filePath);
                    buffer[bufferLength] = '\0';

                    if (sPictureRenderer != NULL)
                        sPictureRenderer->LoadFile(buffer);

                    delete[]buffer;
                }
                else
                {
                    result = RC_InvalidParameters;
                }
                
                
            }
            else
            {
                result = RC_WrongDataSize;
            }

            break;

        case CE_Refresh:

            if (sPictureRenderer != NULL)
                sPictureRenderer->Refresh();
            break;

        }

        return result;
    }
    ResultCode CommandProcessor::ProcessQuery(CommandQuery query, void* commandData, size_t commandSize, void* output_data, size_t output_size)
    {
        if (IsInitialized() == false)
            return ResultCode::RC_NotInitialized;

        ResultCode result = ResultCode::RC_Success;
        switch (query)
        {
        case CQ_GetFileInformation:

            if (output_size == sizeof(QryFileInformation))
            {
                QryFileInformation fileInfo;
                memset(&fileInfo, 0, sizeof(fileInfo));

                if (sPictureRenderer->GetFileInformation(fileInfo) != 0)
                    result = ResultCode::RC_WrongDataSize;
                else
                {
                    QryFileInformation* data = reinterpret_cast<QryFileInformation*>(output_data);
                    *reinterpret_cast<QryFileInformation*>(output_data) = fileInfo;
                }
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