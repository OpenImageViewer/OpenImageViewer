#pragma once
#include <string>
#include <vector>
#include <variant>
#include <list>
#include <map>
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

		
		using Integral = int64_t;
		using Float = long double;
		using String = std::string;

		using SettingValue = std::variant<long double, int64_t, std::string>;

		using MapSettings = std::map<std::string, SettingValue>;

		struct SettingEntryForParsing
		{
			std::string name;
			SettingValue value;
			std::vector<SettingEntryForParsing> children;
		};

		using CommandGroupList = std::vector<CommandGroup>;
		using KeyBindingList = std::vector< KeyBinding>;
		static CommandGroupList LoadCommandGroups();
		static KeyBindingList LoadKeyBindings();
		static MapSettings LoadSettings();
	
	};
}