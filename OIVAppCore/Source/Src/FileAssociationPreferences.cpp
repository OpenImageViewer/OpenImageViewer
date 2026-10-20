#include <OIVAppCore/FileAssociationPreferences.h>
#include <OIVShared/AtomicFile.h>
#include <LLUtils/Exception.h>
#include <LLUtils/StringUtility.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <vector>
#ifdef _WIN32
    #include <Windows.h>
#endif

namespace OIV
{
    namespace
    {
        struct PromptPreferences
        {
            bool suppressAll{};
            std::vector<LLUtils::native_string_type> executables;
            bool Suppresses(const LLUtils::native_string_type& location) const
            {
                return suppressAll ||
                       std::ranges::any_of(executables,
                                           [&](const auto& stored)
                                           {
#ifdef _WIN32
                                               return CompareStringOrdinal(stored.c_str(), -1, location.c_str(), -1,
                                                                           TRUE) == CSTR_EQUAL;
#else
                    return stored == location;
#endif
                                           });
            }
        };
        PromptPreferences LoadPreferences(const std::filesystem::path& path)
        {
            PromptPreferences preferences;
            if (std::filesystem::exists(path))
            {
                std::ifstream input(path, std::ios::binary);
                if (!input)
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Could not open association preferences");
                const auto document     = nlohmann::json::parse(input);
                preferences.suppressAll = document.value("suppressAll", false);
                for (const auto& location : document.value("executables", std::vector<std::string>{}))
                    preferences.executables.push_back(LLUtils::StringUtility::ToNativeString(location));
                if (input.bad())
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Could not read association preferences");
            }
            return preferences;
        }
    }  // namespace
    bool IsAssociationPromptSuppressed(const std::filesystem::path& configDirectory,
                                       const LLUtils::native_string_type& executableLocation)
    {
        return LoadPreferences(configDirectory / "association-prompts.json").Suppresses(executableLocation);
    }
    void SaveAssociationPromptChoice(const std::filesystem::path& configDirectory,
                                     const LLUtils::native_string_type& executableLocation,
                                     FileAssociationChoice choice)
    {
        if (choice != FileAssociationChoice::AskNextTime)
        {
            const auto path  = configDirectory / "association-prompts.json";
            auto preferences = LoadPreferences(path);
            if (choice == FileAssociationChoice::SuppressForUser)
                preferences.suppressAll = true;
            else if (!preferences.Suppresses(executableLocation))
                preferences.executables.push_back(executableLocation);
            nlohmann::json locations = nlohmann::json::array();
            for (const auto& location : preferences.executables)
                locations.push_back(LLUtils::StringUtility::ToAString(location));
            const auto content =
                nlohmann::json{{"suppressAll", preferences.suppressAll}, {"executables", std::move(locations)}}.dump() +
                "\r\n";
            WriteFileAtomically(path, std::as_bytes(std::span(content.data(), content.size())));
        }
    }
}  // namespace OIV
