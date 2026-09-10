#include "ViewerApplication.h"

#include "ViewerApplicationPlatformState.h"

#include <LLUtils/Logging/Logger.h>
#include <LLUtils/PlatformUtility.h>

#include <Windows.h>

#include <filesystem>

namespace OIV
{
    void ViewerApplication::ShowSettings()
    {
        if (settingsContext.created)
        {
            settingsContext.SetVisible(true);
            return;
        }

        using namespace std::filesystem;
        const path programPath     = path(LLUtils::PlatformUtility::GetExeFolder());
        const path netsettingsPath = programPath / "Extensions" / "NetSettings";
        const path cliAdapterPath  = netsettingsPath / "CliAdapter.dll";
        if (!exists(cliAdapterPath))
        {
            LLUtils::Logger::GetSingleton().Log(
                LLUtils::native_string_type(LLUTILS_TEXT("Cannot load Netsettings extension, not found")));
            return;
        }

        SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        fNativeWindowState->settingsDllDirectory = AddDllDirectory(
            (netsettingsPath.lexically_normal().native() + LLUTILS_TEXT("\\")).c_str());
        fNativeWindowState->settingsModule = LoadLibrary(cliAdapterPath.c_str());
        if (fNativeWindowState->settingsModule == nullptr)
        {
            LLUtils::Logger::GetSingleton().Log(
                LLUtils::native_string_type(LLUTILS_TEXT("Cannot load Netsettings extension, error: ")) +
                LLUtils::PlatformUtility::GetLastErrorAsString<wchar_t>());
            return;
        }

        settingsContext.Create = reinterpret_cast<netsettings_Create_func>(
            GetProcAddress(fNativeWindowState->settingsModule, "netsettings_Create"));
        settingsContext.SetVisible = reinterpret_cast<netsettings_SetVisible_func>(
            GetProcAddress(fNativeWindowState->settingsModule, "netsettings_SetVisible"));
        settingsContext.SaveSettings = reinterpret_cast<netsettings_SaveSettings_func>(
            GetProcAddress(fNativeWindowState->settingsModule, "netsettings_SaveUserSettings"));

        GuiCreateParams params{};
        params.userData             = this;
        params.callback             = &ViewerApplication::NetSettingsCallback_;
        const path templateFile     = netsettingsPath / "Resources/GuiTemplate.json";
        params.templateFilePath     = templateFile.c_str();
        const path userSettingsFile = programPath / "Resources/Configuration/Settings.json";
        params.userSettingsFilePath = userSettingsFile.c_str();

        settingsContext.Create(&params);
        settingsContext.SetVisible(true);
        settingsContext.created = true;
    }
}  // namespace OIV
