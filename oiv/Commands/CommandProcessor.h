#pragma once
#include <unordered_map>

#include "API\defs.h"
#include "Interfaces\IPictureRenderer.h"
#include "CommandHandler.h"

namespace OIV
{

    class CommandProcessor
    {
        
    private:
        
        typedef std::unordered_map<CommandExecute, CommandHandler*> MapCommanderHandler;
        static MapCommanderHandler sCommandHandlers;

    public:
        static std::unique_ptr<IPictureRenderer> sPictureRenderer;
        static ResultCode ProcessCommand(CommandExecute command, const std::size_t requestSize, const void* requestData, const std::size_t responseSize, void* responseData);
        static void Log(OIVCHAR* message);
        static bool IsInitialized()
        {
            return sPictureRenderer != nullptr;
        }

    };
}
