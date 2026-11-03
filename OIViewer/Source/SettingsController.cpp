#include "SettingsController.h"
#include "WindowPlacement.h"
#include <LWSSettings/SettingsStorage.hpp>
#include <OIVShared/AtomicFile.h>
#include <cmath>
#include <fstream>
#include <limits>
#include <span>

namespace OIV
{
    namespace
    {
        using Json = nlohmann::json;

        LWSSettings::Status ReadProfile(const std::filesystem::path& path, Json& result)
        {
            try
            {
                std::ifstream input(path, std::ios::binary);
                if (!input)
                    return std::unexpected(LWSSettings::Error{path.string(), "Cannot open settings profile"});
                input >> result;
                if (!result.is_object())
                    return std::unexpected(LWSSettings::Error{path.string(), "Settings profile must be an object"});
                return {};
            }
            catch (const std::exception& error)
            {
                return std::unexpected(LWSSettings::Error{path.string(), error.what()});
            }
        }

        const Json* FindValue(const Json& profile, const std::string& id)
        {
            const Json::json_pointer pointer("/" + id);
            return profile.contains(pointer) ? &profile.at(pointer) : nullptr;
        }

        void CollectValues(const std::vector<LWSSettings::Definition>& definitions, const Json& profile, Json& values)
        {
            for (const auto& definition : definitions)
            {
                if (const auto* group = std::get_if<LWSSettings::GroupDefinition>(&definition.content))
                {
                    CollectValues(group->children, profile, values);
                    continue;
                }
                const auto& setting = std::get<LWSSettings::SettingDefinition>(definition.content);
                const Json* value = FindValue(profile, setting.id);
                if (!value)
                    continue;
                Json normalized = *value;
                if (std::holds_alternative<LWSSettings::IntegerDefinition>(setting.specification) &&
                    normalized.is_number_float())
                {
                    const double number = normalized.get<double>();
                    if (std::isfinite(number) && std::trunc(number) == number &&
                        number >= static_cast<double>(std::numeric_limits<int64_t>::min()) &&
                        number < -static_cast<double>(std::numeric_limits<int64_t>::min()))
                        normalized = static_cast<int64_t>(number);
                }
                values[setting.id] = std::move(normalized);
            }
        }
    }

    LWSSettings::Status SettingsProfile::Load(LWSSettings::SettingsDocument& document,
                                               const std::filesystem::path& path)
    {
        Json profile;
        if (auto result = ReadProfile(path, profile); !result)
            return result;
        try
        {
            Json values = Json::object();
            CollectValues(document.Definitions(), profile, values);
            auto result = LWSSettings::SettingsStorage::LoadValues(document, {{"version", 1}, {"values", values}});
            if (!result)
                return result;
            fOriginal = std::move(profile);
            fBaseline = LWSSettings::SettingsStorage::EncodeValues(document.WorkingValues()).at("values");
            return {};
        }
        catch (const std::exception& error)
        {
            return std::unexpected(LWSSettings::Error{path.string(), error.what()});
        }
    }

    LWSSettings::Status SettingsProfile::Save(const std::filesystem::path& path,
                                               const LWSSettings::Values& values)
    {
        Json profile;
        if (auto result = ReadProfile(path, profile); !result)
            return result;
        try
        {
            const Json updated = LWSSettings::SettingsStorage::EncodeValues(values).at("values");
            for (const auto& [id, value] : updated.items())
            {
                if (fBaseline.contains(id) && fBaseline.at(id) == value)
                    continue;
                const auto* original = FindValue(fOriginal, id);
                const auto* current = FindValue(profile, id);
                if ((original == nullptr) != (current == nullptr) || (original && *original != *current))
                    return std::unexpected(LWSSettings::Error{id, "Setting changed on disk. Reopen settings before saving"});
                profile[Json::json_pointer("/" + id)] = value;
            }
            std::string text = profile.dump(2) + "\n";
            std::string crlf;
            for (char character : text)
            {
                if (character == '\n') crlf += '\r';
                crlf += character;
            }
            WriteFileAtomically(path, std::as_bytes(std::span(crlf.data(), crlf.size())));
            fOriginal = std::move(profile);
            fBaseline = updated;
            return {};
        }
        catch (const std::exception& error)
        {
            return std::unexpected(LWSSettings::Error{path.string(), error.what()});
        }
    }

    SettingsController::SettingsController(LWS::PlatformContext& platform, std::filesystem::path resources,
                                           std::filesystem::path settings, Apply apply)
        : fTemplate(std::move(resources) / "Configuration" / "SettingsTemplate.json"),
          fSettings(std::move(settings)), fApply(std::move(apply))
    {
        LWSSettings::EditorConfig config;
        config.title = "OIViewer settings";
        config.size = {640, 520};
        config.revertOnClose = true;
        config.document.valuesFile = fSettings;
        config.document.writeValues = [this](const auto& path, const auto& values)
        {
            auto result = fProfile.Save(path, values);
            if (result) fSaved = values;
            return result;
        };
        fEditor = std::make_unique<LWSSettings::SettingsEditor>(platform, std::move(config));
        fKeys = fEditor->OnUnhandledKeyDown.Connect([this](const LWSUI::Input& input, bool& handled)
        {
            if (!handled && input.key == LWS::KeyCode::Escape && !input.repeat &&
                !input.control && !input.shift && !input.alt)
            {
                handled = true;
                fEditor->Hide();
            }
        });
        fChange = fEditor->Document().OnChange.Connect([this](const LWSSettings::Change& change)
        {
            if (change.domain == LWSSettings::ValueDomain::Applied && change.phase != LWSSettings::EditPhase::Preview)
            {
                fConfirmed[change.id] = change.newValue;
                fApply(change.id, change.newValue);
            }
        });
        fHidden = fEditor->OnHidden.Connect([this]
        {
            // The file may have changed while edits were pending. Discarding must restore
            // its current values rather than the editor's original saved snapshot.
            const auto previous = fConfirmed;
            if (auto result = LoadProfile(); !result)
            {
                fEditor->Document().OnError.Raise(result.error());
                return;
            }
            for (const auto& [id, value] : fConfirmed)
                if (const auto old = previous.find(id); old == previous.end() || old->second != value)
                    fApply(id, value);
        });
    }

    LWSSettings::Status SettingsController::LoadProfile()
    {
        fLoaded = false;
        if (auto result = fProfile.Load(fEditor->Document(), fSettings); !result)
            return result;
        fConfirmed = fSaved = fEditor->Document().WorkingValues();
        fLoaded = true;
        return {};
    }

    LWSSettings::Status SettingsController::Show(LWS::Window* owner)
    {
        if (!fEditor->IsVisible())
        {
            fLoaded = false;
            auto& document = fEditor->Document();
            if (auto result = LWSSettings::SettingsStorage::LoadTemplateFile(document, fTemplate); !result)
                return result;
            if (auto result = LoadProfile(); !result)
                return result;
        }
        if (auto result = fEditor->Show(); !result) return result;
        const auto activated = fEditor->RequestActivation();
        if (activated != LWS::Result::Success && activated != LWS::Result::NotSupported)
            return std::unexpected(LWSSettings::Error{"window", "Cannot activate settings window"});
        if (owner && fEditor->View() && fEditor->View()->Host())
            std::ignore = PlaceWindowNear(fEditor->View()->Host()->Window(), *owner);
        return {};
    }

    void SettingsController::ApplyPending(const Apply& apply) const
    {
        if (!fLoaded) return;
        for (const auto& [id, value] : fConfirmed)
            if (const auto saved = fSaved.find(id); saved != fSaved.end() && saved->second != value)
                apply(id, value);
    }
}
