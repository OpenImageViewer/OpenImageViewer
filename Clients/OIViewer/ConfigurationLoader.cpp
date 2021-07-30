#include "ConfigurationLoader.h"
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/Exception.h>
#include <nlohmann/json.hpp>

namespace OIV
{
	ConfigurationLoader::CommandGroupList ConfigurationLoader::LoadCommandGroups()
	{
		using namespace nlohmann;
		using namespace LLUtils;

		std::string jsonText = File::ReadAllText(LLUtils::PlatformUtility::GetExeFolder() + LLUTILS_TEXT("./Resources/Configuration/Commands.json"));
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

		std::wstring anchorPath = LLUtils::StringUtility::ToNativeString(LLUtils::PlatformUtility::GetExeFolder()) + L"./Resources/Cursors/arrow-C.cur";
		std::string jsonText = File::ReadAllText(LLUtils::PlatformUtility::GetExeFolder() + LLUTILS_TEXT("./Resources/Configuration/KeyBindings.json"));
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
}


