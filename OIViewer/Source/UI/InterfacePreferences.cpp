#include "InterfacePreferences.h"
#include <OIVShared/AtomicFile.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <span>

namespace OIV
{
    namespace
    {
        constexpr std::string_view Prefix = "viewsettings/interface/";
        constexpr std::pair<std::string_view, bool InterfacePreferences::*> Fields[]{
            {"showborders", &InterfacePreferences::borders},
            {"showmenubar", &InterfacePreferences::menuBar},
            {"showcaption", &InterfacePreferences::caption},
            {"showstatusbar", &InterfacePreferences::statusBar}};
    }  // namespace
    bool InterfacePreferences::Apply(std::string_view id, std::string_view value)
    {
        bool applied = false;
        if (id.starts_with(Prefix) && (value == "true" || value == "false"))
            for (auto [name, member] : Fields)
                if (id.substr(Prefix.size()) == name)
                {
                    this->*member = value == "true";
                    applied       = true;
                }
        return applied;
    }
    std::expected<InterfacePreferences, std::string> LoadInterfacePreferences(const std::filesystem::path& path)
    {
        InterfacePreferences result;
        try
        {
            if (std::filesystem::exists(path))
            {
                std::ifstream file(path);
                const auto profile = nlohmann::json::parse(file);
                for (auto [name, member] : Fields)
                {
                    const auto pointer = nlohmann::json::json_pointer("/viewsettings/interface/" + std::string(name));
                    if (profile.contains(pointer))
                        result.*member = profile.at(pointer).get<bool>();
                }
            }
        }
        catch (const std::exception& error)
        {
            return std::unexpected(error.what());
        }
        return result;
    }
    std::expected<void, std::string> SaveInterfacePreferences(const std::filesystem::path& path,
                                                              const InterfacePreferences& preferences)
    {
        try
        {
            nlohmann::json profile = nlohmann::json::object();
            if (std::filesystem::exists(path))
            {
                std::ifstream file(path);
                profile = nlohmann::json::parse(file);
            }
            auto& settings = profile["viewsettings"]["interface"];
            for (auto [name, member] : Fields)
                settings[name] = preferences.*member;
            const auto text = profile.dump(2) + "\n";
            std::string crlf;
            crlf.reserve(text.size());
            for (char c : text)
            {
                if (c == '\n')
                    crlf += '\r';
                crlf += c;
            }
            WriteFileAtomically(path, std::as_bytes(std::span(crlf.data(), crlf.size())));
        }
        catch (const std::exception& error)
        {
            return std::unexpected(error.what());
        }
        return {};
    }
}  // namespace OIV
