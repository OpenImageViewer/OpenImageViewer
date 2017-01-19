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
            return VERIFY_OPTIONAL_RESPONSE(CmdDataLoadFile, requestSize, CmdResponseLoad, responseSize);
        }

        ResultCode ExecuteImpl(const void* request, const std::size_t requestSize, void* response, const std::size_t responseSize) override
        {
            ResultCode result = RC_UknownError;
            CmdDataLoadFile* dataLoadFile = const_cast<CmdDataLoadFile*>(reinterpret_cast<const CmdDataLoadFile*>(request));

            result = static_cast<ResultCode>(
                CommandProcessor::sPictureRenderer->LoadFile(
                    dataLoadFile->buffer
                    , dataLoadFile->length
                    , dataLoadFile->extension
                    , dataLoadFile->onlyRegisteredExtension));

            //Optional response
            if (result == RC_Success &&  responseSize > 1)
            {
                CmdResponseLoad* loadResponse = reinterpret_cast<CmdResponseLoad*>(response);
                IMCodec::Image* image = CommandProcessor::sPictureRenderer->GetImage();
                loadResponse->width = static_cast<uint32_t>(image->GetWidth());
                loadResponse->height = static_cast<uint32_t>(image->GetHeight());
                loadResponse->bpp = static_cast<uint8_t>(image->GetBitsPerTexel());
                loadResponse->loadTime = image->GetLoadTime();
            }
            return result;
        }
    };


}
