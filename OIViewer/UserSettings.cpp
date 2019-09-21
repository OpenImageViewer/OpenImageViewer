#include "UserSettings.h"
#include <LLUtils/FileHelper.h>

namespace OIV
{
    Serialization::UserSettingsData& UserSettings::getUserSettings()
    {
        return fSettingsData;
    }

    std::wstring UserSettings::GetSettingsPath()
    {
        using namespace std;
        using path = filesystem::path;
        const wstring settingsFileName = L"settings.json";
        path settingsPath = path(LLUtils::PlatformUtility::GetAppDataFolder()) / settingsFileName;
        return settingsPath.wstring();
    }


    void UserSettings::Save()
    {
    #if OIV_VIEWER_BUILD_ESJ == 1
        std::string json = JSON::producer<Serialization::UserSettingsData>::convert(fSettingsData);
        LLUtils::File::WriteAllText(GetSettingsPath(), json);
    #endif
    }

    void UserSettings::Load()
    {
#if OIV_VIEWER_BUILD_ESJ == 1
        using namespace std;
        wstring settingsPath = GetSettingsPath();

        string str = LLUtils::File::ReadAllText(settingsPath);
        if (str.empty() == true)
            Save();
        else
            fSettingsData = JSON::consumer<Serialization::UserSettingsData>::convert(str);
#endif
    }
}
