#pragma once

#include "CommandRegistry.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace OIV
{
    class CommandController
    {
      public:
        using CommandResultSink = std::function<void(const std::wstring&)>;

        explicit CommandController(CommandResultSink resultSink = {})
            : fResultSink(std::move(resultSink))
        {
        }

        void SetResultSink(CommandResultSink resultSink)
        {
            fResultSink = std::move(resultSink);
        }

        template <typename BindingElement>
        void AddConfiguredCommandsAndKeyBindings(LInput::KeyBindings<BindingElement>& keyBindings)
        {
            CommandRegistry::AddConfiguredCommandsAndKeyBindings(fCommandManager, keyBindings);
        }

        void AddCommandCallbacks(const std::vector<CommandRegistry::CommandRegistration>& registrations)
        {
            CommandRegistry::AddCommandCallbacks(fCommandManager, registrations);
        }

        bool ExecutePredefinedCommand(const std::string& commandGroupID)
        {
            CommandManager::CommandRequest commandRequest = fCommandManager.GetCommandRequestGroup(commandGroupID);
            return commandRequest.commandName.empty() == false && ExecuteCommand(commandRequest);
        }

        bool ExecuteCommandInternal(const std::string& commandName, const std::string& arguments)
        {
            CommandManager::CommandRequest request{"", commandName, CommandManager::CommandArgs::FromString(arguments)};
            return ExecuteCommand(request);
        }

        bool ExecuteCommand(const CommandManager::CommandRequest& request)
        {
            CommandManager::CommandResult result;
            if (fCommandManager.ExecuteCommand(request, result))
            {
                if (result.resValue.empty() == false && fResultSink)
                    fResultSink(result.resValue);

                return true;
            }

            return false;
        }

      private:
        CommandManager fCommandManager;
        CommandResultSink fResultSink;
    };
}  // namespace OIV
