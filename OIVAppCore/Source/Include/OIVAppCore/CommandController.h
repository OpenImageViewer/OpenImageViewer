#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/CommandManager.h>

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace OIV
{
    class CommandController
    {
      public:

        struct CommandRegistration
        {
            std::string name;
            CommandManager::CommandCallback callback;
        };

        using CommandResultSink = std::function<void(const LLUtils::native_string_type&)>;

        explicit CommandController(CommandResultSink resultSink = {}) : fResultSink(std::move(resultSink)) {}

        CommandManager& GetCommandManager() { return fCommandManager; }

        void SetResultSink(CommandResultSink resultSink) { fResultSink = std::move(resultSink); }

        void AddCommandCallbacks(const std::vector<CommandRegistration>& registrations)
        {
            for (const CommandRegistration& registration : registrations)
            {
                fCommandManager.AddCommand(CommandManager::Command(registration.name, registration.callback));
            }
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
