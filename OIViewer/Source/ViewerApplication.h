#pragma once
#include "Viewer.h"
#include "UI/ResizeForm.h"
#include <OIVAppCore/FileAssociationSession.h>
#include <LWS/NotificationIconGroup.hpp>
#include <thread>
namespace OIV
{
    // The standalone frontend owns desktop services independently of the reusable Viewer. Its services and
    // session precede the Viewer in destruction order so native callbacks remain valid through surface teardown.
    class SettingsController;
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
        void ApplyEditorSetting(const std::string& id, const std::string& value);
        void ShowSettings();
        void ShowResizeForm();
        void StartResize(IMCodec::ImageSharedPtr source, LLUtils::RectI32 bounds, ViewerImage opened,
                         uint64_t generation, ImageResizeRequest request);
        void InitializeNotificationIcons();
        [[nodiscard]] LWS::Rect GetNotificationIconRect(LWS::NotificationIconGroup::IconID iconId) const;
        void CloseApplication(bool closeToTray);
        void ProcessTopMost();
        void SetTopMostUserMesage();
        void OnNotificationIcon(LWS::NotificationIconGroup::NotificationIconEventArgs args);
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
        std::unique_ptr<SettingsController> fSettings;
        std::unique_ptr<ResizeFormController> fResizeForm;
        // Posted completions use a weak lifetime token; shutdown invalidates it before joining the worker.
        std::shared_ptr<int> fResizeLifetime = std::make_shared<int>(0);
        std::thread fResizeWorker;
        uint64_t fResizeRequestId{};
        bool fResizeBusy{};
    };
}  // namespace OIV
