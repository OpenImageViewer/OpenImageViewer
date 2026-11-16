#include "Viewer.h"
#include "UI/ViewerMenu.h"
#include <LWS/Platform.hpp>

namespace OIV
{
    std::vector<LWSUI::MenuBarItem> Viewer::BuildMainMenu()
    {
        const auto mode     = fWindow.GetWindowMode();
        const bool browsing = fSession.fBrowseSessionController &&
                              fSession.fBrowseSessionController->GetFolderFileList().GetSize() > 0;
        ViewerMenuState state{
            .interfacePreferences = fInterface,
            .image                = IsImageOpen(),
            .selection            = IsImageOpen() && !fSelectionRect.GetSelectionRect().IsEmpty(),
            .file                 = IsOpenedImageIsAFile(),
            .navigation           = browsing,
            .subImages            = IsSubImagesVisible(),
            .showSubImages        = fShowSubImagePanel,
            .fullscreen           = mode == LWS::WindowMode::Fullscreen,
            .multiFullscreen      = mode == LWS::WindowMode::FullscreenAllMonitors,
            .grid                 = fSession.fIsGridEnabled,
            .slideshow            = GetSlideShowEnabled(),
            .canMultiFullscreen   = fPlatform.Supports(LWS::PlatformFeature::MultiMonitorFullscreen).value_or(false),
            .canTray = bool(fShell.close) && fPlatform.Supports(LWS::PlatformFeature::NotificationIcon).value_or(false),
            .canResize   = bool(fShell.showResizeForm),
            .canSettings = bool(fShell.showSettings),
            .canAbout    = bool(fShell.showAbout),
            .sort        = fSession.fFileSorter.GetSortType(),
        };
        return BuildViewerMenu(
            state, fConfiguredCommands, fConfiguredBindings,
            [this](std::string group) { ExecutePredefinedCommand(std::move(group)); }, fShell.commandAllowed);
    }

    void Viewer::ApplyInterface(bool save)
    {
        fWindow.ApplyInterfacePreferences(fInterface);
        fVirtualStatusBar.SetVisible(fInterface.statusBar);
        fWindow.SetShowImageControl(fShowSubImagePanel && IsSubImagesVisible());
        if (save && fShell.saveInterfacePreferences)
            fShell.saveInterfacePreferences(fInterface);
    }

    void Viewer::CMD_About(const CommandManager::CommandRequest&, CommandManager::CommandResult&)
    {
        if (fShell.showAbout)
            fShell.showAbout();
    }

    void Viewer::StartKeyBindingReload()
    {
        const auto directory = fResources / "Configuration";
        const auto file      = directory / "KeyBindings.json";
        std::error_code error;
        const auto modified  = std::filesystem::last_write_time(file, error);
        fKeyBindingsModified = error ? std::nullopt : std::optional{modified};
        std::ignore          = fKeyBindingsReloadTimer.SetTargetWindow(&fWindow.GetWindow());
        fKeyBindingsReloadTimer.SetCallback(MakeSafeCallback(
            [this, directory, file]
            {
                std::error_code error;
                const auto modified = std::filesystem::last_write_time(file, error);
                if (!error && (!fKeyBindingsModified || *fKeyBindingsModified != modified))
                {
                    fKeyBindingsModified = modified;
                    try
                    {
                        auto bindings = ConfigurationLoader::LoadKeyBindings(directory);
                        // Each backend builds a replacement table first. Failed parsing leaves both the
                        // active table and menu labels untouched; commit them together on this UI thread.
                        AddPlatformKeyBindings(bindings);
                        fConfiguredBindings = std::move(bindings);
                        fWindow.RefreshMenu();
                    }
                    catch (const std::exception& failure)
                    {
                        ReportSettingsError("Key bindings: " + std::string(failure.what()));
                    }
                }
            }));
        fKeyBindingsReloadTimer.SetInterval(1000);
    }
}  // namespace OIV
