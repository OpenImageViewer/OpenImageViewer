#pragma once
#include "../CommandHandler.h"
#include <API/defs.h>
#include "Commands/CommandProcessor.h"

namespace OIV
{

    class CommandHandlerLoadFile : public CommandHandler
    {
    protected:
        ResultCode Verify(std::size_t requestSize, std::size_t responseSize) override
        {
            return VERIFY_OPTIONAL_RESPONSE(OIV_CMD_LoadFile_Request, requestSize, OIV_CMD_LoadFile_Response, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ImageHandle handle = ImageNullHandle;
            ResultCode result = RC_UknownError;
            OIV_CMD_LoadFile_Request* dataLoadFile = const_cast<OIV_CMD_LoadFile_Request*>(reinterpret_cast<const OIV_CMD_LoadFile_Request*>(request));
            
                result = static_cast<ResultCode>(
                    CommandProcessor::sPictureRenderer->LoadFile(
                        dataLoadFile->buffer
                        , dataLoadFile->length
                        , dataLoadFile->extension
                        , dataLoadFile->flags & OIV_CMD_LoadFile_Flags::OnlyRegisteredExtension
                        , handle));



                if (result == RC_Success &&  responseSize > 1)
                {
                    OIV_CMD_LoadFile_Response* loadResponse = reinterpret_cast<OIV_CMD_LoadFile_Response*>(response);
                    IMCodec::Image* image = CommandProcessor::sPictureRenderer->GetImage(handle);
                    loadResponse->width = static_cast<uint32_t>(image->GetWidth());
                    loadResponse->height = static_cast<uint32_t>(image->GetHeight());
                    loadResponse->bpp = static_cast<uint8_t>(image->GetBitsPerTexel());
                    loadResponse->loadTime = image->GetData().LoadTime;
                    loadResponse->sizeInMemory = image->GetSizeInMemory();
                    loadResponse->handle = handle;
                }

            
           
            return result;
        }
    };


}
