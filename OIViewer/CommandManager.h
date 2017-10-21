#pragma once
#include <unordered_map>
#include <functional>

namespace OIV
{
    
    class CommandManager
    {
    public:
        
        struct CommandResult
        {
            std::string resValue;
        };

        struct CommandRequest
        {
            std::string description;
            std::string commandName;
            std::string args;
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
        bool ExecuteCommand(const CommandRequest& commandRequest,CommandResult& out_result)
        {
            MapCommands::const_iterator it = fCommands.find(commandRequest.commandName);
            if (it != fCommands.end())
            {
                fCommands[commandRequest.commandName].Execute(commandRequest, out_result);
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
