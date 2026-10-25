#pragma once

#include <OIVAppCore/ConfigurationLoader.h>

#include <OIVAppCore/CommandManager.h>

#include <string>

namespace OIV
{
    class CommandRegistry
    {
      public:

        static void AddConfiguredCommands(CommandManager& commandManager,
                                          const ConfigurationLoader::CommandGroupList& commandGroups)
        {
            for (const auto& commandGroup : commandGroups)
            {
                commandManager.AddCommandGroup({commandGroup.commandGroupID, commandGroup.commandDisplayName,
                                                commandGroup.commandName, commandGroup.arguments});
            }
        }
    };
}  // namespace OIV
