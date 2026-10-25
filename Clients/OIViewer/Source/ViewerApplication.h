#pragma once
#include "Viewer.h"
#include <OIVAppCore/FileAssociationSession.h>
#include <LWS/NotificationIconGroup.hpp>
#include <NetSettings/GuiProvider.h>
namespace OIV
{
    // The standalone frontend owns desktop services independently of the reusable Viewer. Its services and
    // session precede the Viewer in destruction order so native callbacks remain valid through surface teardown.
    class ViewerApplication final
    {
      public:

        ViewerApplication(LWS::PlatformContext& platform, const UserDirectories& directories);
        ~ViewerApplication();
        void Init(LLUtils::native_string_type path, const RendererOptions& options = {});
        LWS::LoopResult Run();
        static LWS::Handle FindTrayBarWindow();

      private:

        void InitializeShellState();
        LWS::WindowConfig WindowConfig() const;
        void StartFileAssociationOperation(FileAssociationAction action);
        void StopFileAssociationOperation();
        void CMD_FileAssociations(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CloseFileAssociationDialog();
        void HandleFileAssociationNotice(const FileAssociationNotice& notice);
        void ShowSettings();
        static void NetSettingsCallback_(ItemChangedArgs* callback);
        void NetSettingsCallback(ItemChangedArgs* callback);
        void InitializeNotificationIcons();
        [[nodiscard]] LWS::Rect GetNotificationIconRect(LWS::NotificationIconGroup::IconID iconId) const;
        void CloseApplication(bool closeToTray);
        void ProcessTopMost();
        void SetTopMostUserMesage();
        void OnNotificationIcon(LWS::NotificationIconGroup::NotificationIconEventArgs args);
        using netsettings_Create_func       = void (*)(GuiCreateParams*);
        using netsettings_SetVisible_func   = void (*)(bool);
        using netsettings_SaveSettings_func = void (*)();

        struct SettingsContext
        {
            bool created;
            netsettings_Create_func Create;
            netsettings_SetVisible_func SetVisible;
            netsettings_SaveSettings_func SaveSettings;
        } settingsContext{};

        struct NativeWindowState;
        LWS::PlatformContext& fPlatform;
        const UserDirectories& fUserDirectories;
        ViewerSession fSession;
        std::unique_ptr<FileAssociationSession> fFileAssociations;
        uint64_t fAssociationRequestId{};
        std::unique_ptr<NativeWindowState> fNativeWindowState;
        LWS::NotificationIconGroup::IconID fNotificationIconID;
        std::unique_ptr<ContextMenu<int>> fNotificationContextMenu;
        int fTopMostCounter = 0;
        LWS::Timer fTimerTopMostRetention;
        Viewer fViewer;
    };
}  // namespace OIV
