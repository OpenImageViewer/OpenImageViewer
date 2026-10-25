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

        enum class DispatchResult
        {
            Unknown,
            Blocked,
            Dispatched
        };
        using RequestPredicate = bool (*)(const CommandManager::CommandRequest&);

        using CommandResultSink = std::function<void(const LLUtils::native_string_type&)>;

        explicit CommandController(CommandResultSink resultSink = {}) : fResultSink(std::move(resultSink)) {}

        CommandManager& GetCommandManager() { return fCommandManager; }

        void SetRequestPredicate(RequestPredicate predicate) { fRequestPredicate = predicate; }

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
            return DispatchCommand(request) != DispatchResult::Unknown;
        }

        DispatchResult DispatchCommand(const CommandManager::CommandRequest& request)
        {
            // A blocked known command is still recognized. Input routing must not forward half a gesture
            // to the host merely because its action is disabled in the current frontend.
            auto* command = fCommandManager.FindCommand(request.commandName);
            if (!command)
                return DispatchResult::Unknown;
            if (fRequestPredicate && !fRequestPredicate(request))
                return DispatchResult::Blocked;
            CommandManager::CommandResult result;
            command->Execute(request, result);
            if (!result.resValue.empty() && fResultSink)
                fResultSink(result.resValue);
            return DispatchResult::Dispatched;
        }

      private:

        RequestPredicate fRequestPredicate{};
        CommandManager fCommandManager;
        CommandResultSink fResultSink;
    };
}  // namespace OIV
