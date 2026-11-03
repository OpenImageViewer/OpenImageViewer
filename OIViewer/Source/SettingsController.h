#pragma once
#include <LWSSettings/SettingsEditor.hpp>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <functional>
#include <memory>

namespace OIV
{
    class SettingsProfile
    {
      public:
        LWSSettings::Status Load(LWSSettings::SettingsDocument& document, const std::filesystem::path& path);
        LWSSettings::Status Save(const std::filesystem::path& path, const LWSSettings::Values& values);

      private:
        nlohmann::json fOriginal;
        nlohmann::json fBaseline;
    };

    class SettingsController
    {
      public:
        using Apply = std::function<void(const std::string&, const LWSSettings::Value&)>;
        SettingsController(LWS::PlatformContext& platform, std::filesystem::path resources,
                           std::filesystem::path settings, Apply apply);
        LWSSettings::Status Show(LWS::Window* owner = nullptr);
        void ApplyPending(const Apply& apply) const;

      private:
        LWSSettings::Status LoadProfile();
        std::filesystem::path fTemplate;
        std::filesystem::path fSettings;
        SettingsProfile fProfile;
        Apply fApply;
        bool fLoaded = false;
        LWSSettings::Values fConfirmed;
        LWSSettings::Values fSaved;
        std::unique_ptr<LWSSettings::SettingsEditor> fEditor;
        LWSUI::Event<void(const LWSSettings::Change&)>::Connection fChange;
        LWSUI::Event<void()>::Connection fHidden;
        LWSUI::Event<void(const LWSUI::Input&, bool&)>::Connection fKeys;
    };
}
