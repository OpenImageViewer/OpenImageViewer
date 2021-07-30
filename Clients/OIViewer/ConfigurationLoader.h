#pragma once
#include <string>
#include <vector>
namespace OIV
{

	class ConfigurationLoader
	{
	public:
		struct CommandGroup
		{
			std::string commandGroupID;
			std::string commandDisplayName;
			std::string commandName;
			std::string arguments;
		};

		struct KeyBinding
		{
			std::string KeyCombinationName;
			std::string GroupID;
		};

		using CommandGroupList = std::vector<CommandGroup>;
		using KeyBindingList = std::vector< KeyBinding>;
		static CommandGroupList LoadCommandGroups();
		static KeyBindingList LoadKeyBindings();
	
	};
}