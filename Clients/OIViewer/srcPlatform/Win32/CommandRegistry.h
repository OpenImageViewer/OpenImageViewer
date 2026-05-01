#pragma once

#include "ConfigurationLoader.h"

#include <LInput/Keys/KeyBindings.h>
#include <LInput/Keys/KeyCombination.h>

#include <oivappcore/CommandManager.h>

#include <string>
#include <vector>

namespace OIV
{
    class CommandRegistry
    {
      public:
        struct CommandRegistration
        {
            std::string name;
            CommandManager::CommandCallback callback;
        };

        template <typename BindingElement>
        static void AddConfiguredCommandsAndKeyBindings(CommandManager& commandManager,
                                                        LInput::KeyBindings<BindingElement>& keyBindings)
        {
            auto commandGroups = ConfigurationLoader::LoadCommandGroups();
            auto configuredKeyBindings = ConfigurationLoader::LoadKeyBindings();

            for (const auto& commandGroup : commandGroups)
            {
                commandManager.AddCommandGroup({commandGroup.commandGroupID, commandGroup.commandDisplayName,
                                                commandGroup.commandName, commandGroup.arguments});
            }

            for (const auto& keyBinding : configuredKeyBindings)
            {
                keyBindings.AddBinding(LInput::KeyCombination::FromString(keyBinding.KeyCombinationName),
                                       {keyBinding.GroupID, std::string(), std::string()});
            }
        }

        static void AddCommandCallbacks(CommandManager& commandManager,
                                        const std::vector<CommandRegistration>& registrations)
        {
            for (const auto& registration : registrations)
            {
                commandManager.AddCommand(CommandManager::Command(registration.name, registration.callback));
            }
        }
    };
}  // namespace OIV
