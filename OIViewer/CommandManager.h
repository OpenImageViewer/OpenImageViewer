#pragma once
#include <unordered_map>
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

        struct CommandResult
        {
            std::string resValue;
        };

        struct CommandArgs
        {
            static CommandArgs FromString(const std::string& str)
            {
                
                using namespace LLUtils;
                using namespace std;
                CommandArgs args;
                ListAString props = StringUtility::split(str, ';');
                args.args.reserve(props.size());

                for (const std::string& pair : props)
                {
                    ListAString keyval = StringUtility::split(pair, '=');
                    args.args.push_back({ keyval[0],keyval[1] });
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

        struct CommandClientRequest
        {
            std::string description;
            std::string commandName;
            std::string args;
        };

        struct CommandRequest
        {
            std::string description;
            std::string commandName;
            CommandArgs args;
        };
        

        using CommandCallback = std::function<void(const CommandRequest&, CommandResult&)>;


        class Command
        {
        public:

            Command()
            {
                
            }
            Command(const std::string& commandName, CommandCallback callback)
            {
                fCommandName = commandName;
                fCallBack = callback;
            }
            const std::string& GetName() const
            {
                return fCommandName;
            }
            void Execute(const CommandRequest& commandRequest, CommandResult& res)
            {
                fCallBack(commandRequest, res);
            }

        private:
            std::string fCommandName;
            CommandCallback fCallBack;
        };

        
        
        using MapCommands = std::unordered_map<std::string, Command>;
    public:
        bool ExecuteCommand(const CommandClientRequest& commandRequest,CommandResult& out_result)
        {
            MapCommands::iterator it = fCommands.find(commandRequest.commandName);
            if (it != fCommands.end())
            {
                CommandRequest CommandRequestParsed;
                CommandRequestParsed.description = commandRequest.description;
                CommandRequestParsed.commandName = commandRequest.commandName;
                CommandRequestParsed.args = CommandArgs::FromString(commandRequest.args);

                it->second.Execute(CommandRequestParsed, out_result);
                return true;
            }
            return false;
        }
        void AddCommand(const Command& command)
        {
            fCommands.insert(std::make_pair(command.GetName(), command));
        }
    private:
        MapCommands fCommands;
    };
}
