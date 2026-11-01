#include "ViewerApplication.h"
#include "ViewerResources.h"
#include "ViewerApplicationPlatformState.h"
#include <algorithm>
#include <cmath>
namespace OIV
{
    ViewerApplication::ViewerApplication(LWS::PlatformContext& platform, const UserDirectories& directories)
        : fPlatform(platform), fUserDirectories(directories),
          fSession(CreateViewerFileWatcher(), [&platform](std::move_only_function<void()> task)
                   { return platform.PostTask(std::move(task)) == LWS::Result::Success; }),
          fTimerTopMostRetention(platform),
          fViewer(platform, fSession, directories, ResolveViewerResources(directories),
                  {
                      .created = [this]
                      { std::ignore = fViewer.fWindow.GetWindow().Center(LWS::CenterTarget::PrimaryMonitor); },
                      .ready =
                          [this]
                      {
#ifdef LWS_HAS_WIN32_BACKEND
                          fViewer.EnableRawMouse();
#endif
                          InitializeNotificationIcons();
                          fNotificationContextMenu = std::make_unique<ContextMenu<int>>(fViewer.fWindow.GetWindow());
                          fNotificationContextMenu->AddItem(LLUTILS_TEXT("Quit"), int{});
                          std::ignore = fTimerTopMostRetention.SetTargetWindow(&fViewer.fWindow.GetWindow());
                          fTimerTopMostRetention.SetCallback(fViewer.MakeSafeCallback([this] { ProcessTopMost(); }));
                          StartFileAssociationOperation(FileAssociationAction::Check);
                      },
                      // Unsubscribe native desktop callbacks while the viewer still exists. These services precede
                      // the viewer in ownership order and can otherwise outlive its C++ subobject during teardown.
                      .stop =
                          [this]
                      {
                          StopFileAssociationOperation();
                          fTimerTopMostRetention.SetInterval(0);
#ifdef LWS_HAS_WIN32_BACKEND
                          if (fNativeWindowState)
                              fNativeWindowState->notificationSubscription.Unsubscribe();
#endif
                          fNotificationContextMenu.reset();
                      },
                      .close        = [this](bool tray) { CloseApplication(tray); },
                      .destroyed    = [this] { fPlatform.RequestQuit(); },
                      .showSettings = [this] { ShowSettings(); },
                      .topMost =
                          [this]
                      {
                          if (fViewer.fWindow.GetWindow().SetAlwaysOnTop(true) == LWS::Result::Success)
                          {
                              fTopMostCounter = 3;
                              SetTopMostUserMesage();
                              fTimerTopMostRetention.SetInterval(1000);
                          }
                      },
                      .openExternalFile =
                          [this](LLUtils::native_string_type path)
                      {
                          fViewer.LoadFile(std::move(path), IMCodec::PluginTraverseMode::SuitablePlugin);
                          std::ignore = fViewer.fWindow.GetWindow().SetVisible(true);
                      },
                      .fileAssociations = [this](const auto& request, auto& result)
                      { CMD_FileAssociations(request, result); },
                      .associationNotice = [this](const auto& notice) { HandleFileAssociationNotice(notice); },
                  })
    {
        InitializeShellState();
    }
    ViewerApplication::~ViewerApplication()
    {
        fViewer.Stop();
    }

    void ViewerApplication::ShowSettings()
    {
        // NetSettings is unsupported; retain the command callback as a no-op.
    }

    void ViewerApplication::Init(LLUtils::native_string_type path, const RendererOptions& options)
    {
        fViewer.Init(std::move(path), options, WindowConfig());
    }
    LWS::LoopResult ViewerApplication::Run()
    {
        return fPlatform.RunMessageLoop();
    }
    LWS::WindowConfig ViewerApplication::WindowConfig() const
    {
        // initialize the windowing system of the window
        // The historical 1200x800 native window was calibrated at 125% scaling. LWS uses 96-DPI logical units, so
        // this client estimate preserves that reference footprint while remaining portable.
        LWS::LogicalSize initialClientSize{946, 602};
        if (const auto monitor = fPlatform.GetPrimaryMonitor(); monitor.has_value())
        {
            double workWidth  = monitor->workRect.GetWidth();
            double workHeight = monitor->workRect.GetHeight();
#ifdef LWS_HAS_WIN32_BACKEND
            workWidth /= monitor->contentScale.x;
            workHeight /= monitor->contentScale.y;
#endif
            constexpr double edgeMargins = 64.0;
            const double scale           = std::min({1.0, std::max(workWidth - edgeMargins, 1.0) / initialClientSize.x,
                                                     std::max(workHeight - edgeMargins, 1.0) / initialClientSize.y});
            initialClientSize            = {
                std::max(1, static_cast<int32_t>(std::lround(initialClientSize.x * scale))),
                std::max(1, static_cast<int32_t>(std::lround(initialClientSize.y * scale))),
            };
        }
        return {
            .clientSize         = initialClientSize,
            .styles             = LWS::WindowStyleFlags(Viewer::WindowChromeStyles),
            .backgroundColor    = LLUtils::Color(45, 45, 48),
            .dragAndDropEnabled = fPlatform.Supports(LWS::PlatformFeature::DragAndDrop).value_or(false),
        };
    }
}  // namespace OIV
