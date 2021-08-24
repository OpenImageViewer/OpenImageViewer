#include "ConfigurationLoader.h"
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/Exception.h>
#include <LLUtils/Logging/Logger.h>
#include <nlohmann/json.hpp>
#include <stack>

namespace OIV
{
	ConfigurationLoader::CommandGroupList ConfigurationLoader::LoadCommandGroups()
	{
		using namespace nlohmann;
		using namespace LLUtils;

		std::string jsonText = File::ReadAllText<std::string>(LLUtils::PlatformUtility::GetExeFolder() + LLUTILS_TEXT("./Resources/Configuration/Commands.json"));
		auto jsonObject = json::parse(jsonText);
		auto commands = jsonObject["commands"];

		if (commands == nullptr || commands.is_array() == false)
			LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "File contents mismatch");

		CommandGroupList commandsList;
		for (auto commandGroup : commands)
			commandsList.push_back({ commandGroup["GroupID"], commandGroup["DisplayName"] ,commandGroup["Name"], commandGroup["arguments"] });

		return commandsList;
	}

	ConfigurationLoader::KeyBindingList ConfigurationLoader::LoadKeyBindings()
	{
		using namespace nlohmann;
		using namespace LLUtils;

		std::string jsonText = File::ReadAllText<std::string>(LLUtils::PlatformUtility::GetExeFolder() + LLUTILS_TEXT("./Resources/Configuration/KeyBindings.json"));
		auto jsonObject = json::parse(jsonText);
		auto keyBindings = jsonObject["KeyBindings"];

		if (keyBindings == nullptr || keyBindings.is_array() == false)
			LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "File contents mismatch");

		KeyBindingList keyBindingsList;
		for (auto& keybindingPair : keyBindings.items())
		{
			
			for (auto& [key, value] : keybindingPair.value().items())
				keyBindingsList.push_back(KeyBinding{ key, value.get<std::string>() });
		}

		return keyBindingsList;
	}

	ConfigurationLoader::MapSettings ConfigurationLoader::LoadSettings()
	{
		using namespace nlohmann;
		using namespace LLUtils;

		MapSettings mapSettings;

		try
		{
			std::string jsonText = File::ReadAllText<std::string>(LLUtils::PlatformUtility::GetExeFolder() + LLUTILS_TEXT("./Resources/Configuration/Settings.json"));
			auto jsonObject = json::parse(jsonText);

			SettingEntryForParsing root;


			using StackData = std::tuple<json*, SettingEntryForParsing*, std::string>;

			std::stack<StackData> stck;
			

			stck.emplace(&jsonObject, &root, "");

			while (stck.empty() == false)
			{
				const auto [jsonTree, settingEntry, currentNamespace] = stck.top();
				stck.pop();

				struct TmpEntry
				{
					bool isObject = false;
					std::string name;
					json* child;
					SettingEntryForParsing entry;
				};

				std::list< TmpEntry> tmpList;

				for (auto& child : jsonTree->items())
				{
					tmpList.push_back(TmpEntry());

					auto& currentTmp = tmpList.back();
					auto& currentChild = tmpList.back().entry;
					currentChild.name = child.key();
					if (child.value().is_object() == true)
					{
						currentTmp.isObject = true;
						currentTmp.child = &child.value();
						currentTmp.name = child.key();


						//stck.emplace(&child.value(), &currentChild);
					}
					else if (child.value().is_boolean())
						currentChild.value = child.value().get<Bool>();
					else if (child.value().is_number_integer())
						currentChild.value = child.value().get<Integral>();
					else if (child.value().is_number_float())
						currentChild.value = child.value().get<Float>();
					else if (child.value().is_string())
						currentChild.value = child.value().get<String>();


					if (child.value().is_object() == false)
					{
						mapSettings.emplace(currentNamespace + '/' + currentChild.name, currentChild.value);
					}
				}

				settingEntry->children.resize(tmpList.size());

				decltype(tmpList)::const_iterator it = tmpList.begin();
				for (size_t i = 0; i < tmpList.size(); i++, it++)
				{

					settingEntry->children.at(i) = it->entry;
					if (it->isObject)
					{
						stck.emplace(it->child, &settingEntry->children.at(i), currentNamespace + '/' + it->name);
					}
				}

				//settingEntry ->children = 
			}
		}
		catch (nlohmann::detail::exception& exception)
		{
			LLUtils::Logger::GetSingleton().Log(exception.what());
		}
		catch (...)
		{
			
		}


		


		return mapSettings;
	}

}


