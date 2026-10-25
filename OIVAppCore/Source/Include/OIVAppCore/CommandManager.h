#pragma once

#include <LLUtils/StringDefs.h>
#include <LLUtils/StringUtility.h>

#include <string>
#include <vector>
#include <map>
#include <functional>

namespace OIV
{

    class CommandManager
    {
      public:

        struct KeyValuePair
        {
            std::string key;
            std::string value;
        };

        using ListKeyValue = std::vector<KeyValuePair>;

        struct CommandGroup
        {
            std::string GroupID;
            std::string commandDisplayName;
            std::string commandName;
            std::string arguments;
        };

        using MapCommandGroup = std::map<std::string, CommandGroup>;

        struct CommandResult
        {
            LLUtils::native_string_type resValue;
        };

        struct CommandArgs
        {
            static CommandArgs FromString(const std::string& str)
            {
                CommandArgs args;
                const LLUtils::ListAString props = LLUtils::StringUtility::split(str, ';');
                args.args.reserve(props.size());

                for (const std::string& pair : props)
                {
                    const std::size_t separator = pair.find('=');
                    if (separator != std::string::npos)
                        args.args.push_back({pair.substr(0, separator), pair.substr(separator + 1)});
                }
                return args;
            }

            ListKeyValue args;
            std::string GetArgValue(const std::string argName) const
            {
                for (const KeyValuePair& arg : args)
                    if (arg.key == argName)
                        return arg.value;

                return std::string();
            }
        };

        struct CommandRequest
        {
            std::string displayName;
            std::string commandName;
            CommandArgs args;
        };

        using CommandCallback = std::function<void(const CommandRequest&, CommandResult&)>;

        class Command
        {
          public:

            Command() {}
            Command(const std::string& commandName, CommandCallback callback)
            {
                fCommandName = commandName;
                fCallBack    = callback;
            }
            const std::string& GetName() const { return fCommandName; }
            void Execute(const CommandRequest& commandRequest, CommandResult& res) { fCallBack(commandRequest, res); }

          private:

            std::string fCommandName;
            CommandCallback fCallBack;
        };

        using MapCommands = std::map<std::string, Command>;

      public:

        CommandRequest GetCommandRequestGroup(const std::string& commandGroupID)
        {
            auto itCommadnGroup = fMapCommandGroup.find(commandGroupID);
            if (itCommadnGroup != fMapCommandGroup.end())
            {
                const auto& group = itCommadnGroup->second;
                return CommandRequest{group.commandDisplayName, group.commandName,
                                      CommandArgs::FromString(group.arguments)};
            }
            return {};
        }

        Command* FindCommand(const std::string& name)
        {
            const auto it = fCommands.find(name);
            return it != fCommands.end() ? &it->second : nullptr;
        }

        bool ExecuteCommand(const CommandManager::CommandRequest& request, CommandResult& out_result)
        {
            if (auto* command = FindCommand(request.commandName))
            {
                command->Execute(request, out_result);
                return true;
            }
            return false;
        }

        void AddCommandGroup(const CommandGroup& commandGroup)
        {
            fMapCommandGroup.emplace(commandGroup.GroupID, commandGroup);
        }

        void AddCommand(const Command& command) { fCommands.insert(std::make_pair(command.GetName(), command)); }

        const MapCommandGroup& GetPredefinedCommandGroup() const { return fMapCommandGroup; }

      private:

        MapCommandGroup fMapCommandGroup;
        MapCommands fCommands;
    };
}  // namespace OIV
