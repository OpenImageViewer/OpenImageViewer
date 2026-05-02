#pragma once

#include "ConfigurationLoader.h"

#include <LInput/Keys/KeyBindings.h>
#include <LInput/Keys/KeyCombination.h>

#include <OIVAppCore/CommandManager.h>

#include <string>

namespace OIV
{
    class CommandRegistry
    {
      public:
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
    };
}  // namespace OIV
