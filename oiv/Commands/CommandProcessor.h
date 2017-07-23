#pragma once
#include <unordered_map>

#include "API\defs.h"
#include "Interfaces\IPictureRenderer.h"
#include "CommandHandler.h"

namespace OIV
{

    class CommandProcessor
    {
    private: // types
        typedef std::unordered_map<CommandExecute, CommandHandler*> MapCommanderHandler;
    public: // methods
        CommandProcessor();
        ~CommandProcessor();
        ResultCode ProcessCommand(CommandExecute command, const std::size_t requestSize, const void* requestData, const std::size_t responseSize, void* responseData);
    private: // methods
        //bool IsInitialized() const;

    private: // member fields
        MapCommanderHandler fCommandHandlers;

    };
}
