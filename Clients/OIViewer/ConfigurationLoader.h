#pragma once
#include <string>
#include <vector>
#include <variant>
#include <list>
#include <map>
#include <cstdint>
namespace OIV
{

	class ConfigurationLoader
	{
	public:

		static constexpr char seperator = '/';
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

		enum class ValueType
		{
			 Integral 
			,Float 
			,String 
			,Bool
		};

		using Integral = int64_t;
		using Float = long double;
		using String = std::string;
		using Bool = bool;

		using SettingValue = std::variant<bool, int64_t, long double, std::string>;

		using MapSettings = std::map<std::string, std::string>;

		struct SettingEntryForParsing
		{
			std::string name;
			std::string value;
			std::vector<SettingEntryForParsing> children;
		};

		using CommandGroupList = std::vector<CommandGroup>;
		using KeyBindingList = std::vector< KeyBinding>;
		static CommandGroupList LoadCommandGroups();
		static KeyBindingList LoadKeyBindings();
		static MapSettings LoadSettings();
	
	};
}