#pragma once
#include <unordered_map>

#include <defs.h>
#include "../IPictureRenderer.h"
#include "CommandHandler.h"

namespace OIV
{

    class CommandProcessor
    {
    private: // types
        typedef std::unordered_map<CommandExecute, std::unique_ptr<CommandHandler>> MapCommanderHandler;
    public: // methods
        CommandProcessor();
        ResultCode ProcessCommand(CommandExecute command, const std::size_t requestSize, const void* requestData, const std::size_t responseSize, void* responseData);
    private: // methods
        //bool IsInitialized() const;

    private: // member fields
        MapCommanderHandler fCommandHandlers;

    };
}
