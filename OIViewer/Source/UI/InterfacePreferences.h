#pragma once
#include <filesystem>
#include <expected>
#include <string>
#include <string_view>

namespace OIV
{
    struct InterfacePreferences
    {
        bool borders = true, menuBar = true, statusBar = false, caption = true, welcomeMessage = true;
        bool operator==(const InterfacePreferences&) const = default;
        bool Apply(std::string_view id, std::string_view value);
    };
    std::expected<InterfacePreferences, std::string> LoadInterfacePreferences(const std::filesystem::path& path);
    std::expected<void, std::string> SaveInterfacePreferences(const std::filesystem::path& path,
                                                              const InterfacePreferences& preferences);
}  // namespace OIV
