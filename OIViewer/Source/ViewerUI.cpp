#include <iomanip>
#include <filesystem>
#include <thread>
#include <future>
#include <cassert>

#include "Viewer.h"
#include "HandledException.h"

#include <Version.h>

#include <LWS/Platform.hpp>
#ifdef LWS_HAS_WIN32_BACKEND
    #include <LWS/Win32/WindowExtensions.hpp>
#elif defined(LWS_HAS_WAYLAND_BACKEND)
    #include <LWS/Wayland/WindowExtensions.hpp>
#endif

#include <LLUtils/Exception.h>
#include <LLUtils/FileHelper.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/StringUtility.h>
#include <LLUtils/UniqueIDProvider.h>
#include <LLUtils/FileSystemHelper.h>
#include <LLUtils/Rect.h>

#include <OIVAppCore/OIVHelper.h>
#include "Helpers/ClipboardSetup.h"
#include <OIVAppCore/MessageFormatter.h>
#include <OIVAppCore/MessageHelper.h>
#include <OIVAppCore/ShellIntegrationHelper.h>
#include "Helpers/ShellCommandHandler.h"

#include "VirtualStatusBar.h"
#include "MonitorProvider.h"

#include "ContextMenu.h"
#include "Globals.h"
#include <OIVAppCore/ConfigurationLoader.h>
#include "CommandRegistry.h"
#include <OIVAppCore/ColorCorrectionCommandPolicy.h>
#include <OIVAppCore/FileChangePolicy.h>
#include <OIVAppCore/FrameLimiterPolicy.h>
#include <OIVAppCore/ImageEditPolicy.h>
#include <OIVAppCore/ImageFormatCatalogPolicy.h>
#include <OIVAppCore/ImageLoadPresentationPolicy.h>
#include <OIVAppCore/ImageTransformCommandPolicy.h>
#include <OIVAppCore/InputGesturePolicy.h>
#include <OIVAppCore/SelectionWorkflowPolicy.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVAppCore/SortCommandPolicy.h>
#include <OIVAppCore/SubImagePolicy.h>
#include <OIVAppCore/ViewActionController.h>
#include <OIVAppCore/ViewCommandPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <ImageUtil/ImageUtil.h>
#include <OIVAppCore/ViewerCompletion.h>
#include "ViewerMouseInput.h"

namespace OIV
{
    Viewer::~Viewer()
    {
        Stop();
        // Keep application members and listener registrations alive throughout native window cleanup.
        std::ignore = fWindow.Destroy();
    }

    void Viewer::SetImageInfoVisible(bool visible)
    {
        if (visible != fImageInfoVisible)
        {
            fImageInfoVisible = visible;

            if (fImageInfoVisible == true)
            {
                ShowImageInfo();
            }
            else
            {
                TextLabel* text = fLabelManager.GetTextLabel("imageInfo");
                if (text != nullptr)
                {
                    fLabelManager.Remove("imageInfo");
                    fRefreshOperation.Queue();
                }
            }
        }
    }

    bool Viewer::GetImageInfoVisible() const
    {
        return fImageInfoVisible;
    }

    void Viewer::Stop()
    {
        // Stop callbacks before native rendering, including timers entered through nested dispatch.
        // The renderer owner stays alive until its image registrations and queued results are released.
        if (!fIsShuttingDown.exchange(true))
        {
            fUiLifetime.reset();
            if (fSessionAttached)
            {
                fSession.Detach();
                fSessionAttached = false;
            }
            if (fShell.stop)
                fShell.stop();
            fRefreshTimer.Enable(false);
            fSequencerTimer.SetInterval(0);
            fTimerSlideShow.SetInterval(0);
            fTimerNoActiveZoom.SetInterval(0);
            fMouseInput->Cancel();
            if (fAutoScroll != nullptr && fAutoScroll->IsAutoScrolling())
                fAutoScroll->ToggleAutoScroll();
            if (fMessageManager)
                fMessageManager->Stop();
            // Presentation owners release while their renderer and native handles are still available.
            fAutoScrollAnchor.reset();
            fLabelManager.RemoveAll();
            fResampledRenderImage.reset();
            fMainRenderImage.reset();
            if (fRenderer != nullptr)
                fRenderer->Shutdown();
        }
    }

    bool Viewer::HandleEventCallback(const std::function<bool()>& callback) noexcept
    {
        if (fIsShuttingDown)
            return true;
        try
        {
            return callback();
        }
        catch (...)
        {
            ReportHandledException(std::current_exception());
            return true;
        }
    }

    std::function<void()> Viewer::MakeSafeCallback(std::function<void()> callback)
    {
        return [this, lifetime = fUiWeakLifetime, callback = std::move(callback)]()
        {
            // Normal platform shutdown drains tasks after the viewer is destroyed. Both run on the UI thread,
            // so checking the token before any owner access is sufficient. It need not retain the viewer.
            if (!lifetime.expired())
                HandleEventCallback(
                    [&]()
                    {
                        callback();
                        return true;
                    });
        };
    }

    Viewer::Viewer(LWS::PlatformContext& platform, ViewerSession& session, const UserDirectories& directories,
                   ViewerResources resources, ViewerShell shell)
        : fShell(std::move(shell)), fPlatform(platform), fSession(session), fResources(std::move(resources.directory)),
          fSettingsFile(std::move(resources.settingsFile)), fModulePath(std::move(resources.modulePath)),
          fUserDirectories(directories), fRefreshTimer(platform,
                                                       [this]()
                                                       {
                                                           HandleEventCallback(
                                                               [this]()
                                                               {
                                                                   OnRefreshTimer();
                                                                   return true;
                                                               });
                                                       }),
          fWindow(platform, fResources), fRefreshOperation(
                                             [this]()
                                             {
                                                 HandleEventCallback(
                                                     [this]()
                                                     {
                                                         OnRefresh();
                                                         return true;
                                                     });
                                             }),
          fPreserveImageSpaceSelection(
              [this]()
              {
                  HandleEventCallback(
                      [this]()
                      {
                          OnPreserveSelectionRect();
                          return true;
                      });
              }),
          fSelectionRect(
              [this](const LLUtils::RectI32& rect, bool visible)
              {
                  HandleEventCallback(
                      [&]()
                      {
                          OnSelectionRectChanged(rect, visible);
                          return true;
                      });
              }),
          fVirtualStatusBar(&fLabelManager,
                            [this]()
                            {
                                HandleEventCallback(
                                    [this]()
                                    {
                                        OnLabelRefreshRequest();
                                        return true;
                                    });
                            }),
          fTimerSlideShow(platform), fClipboardHelper(platform), fTimerNoActiveZoom(platform),
          fTimerNavigation(platform), fFreeType(std::make_unique<FreeType::FreeTypeConnector>()),
          fLabelManager(fFreeType.get(), fResources / "Fonts" / "CascadiaCode.ttf"), fContextMenuTimer(platform),
          fSequencerTimer(platform)

    {
        InitializePlatformState();
        fMouseInput = std::make_unique<ViewerMouseInput>(*this);
        fCommandController.SetResultSink([this](const LLUtils::native_string_type& message)
                                         { SetUserMessage(message); });

        // LLUtils::Exception::SetThrowErrorsInDebug(false);
        fMonitorSubscription = fMonitorProvider.Changed.Subscribe(
            std::bind(&Viewer::OnMonitorChanged, this, std::placeholders::_1));
    }

    void Viewer::OnLabelRefreshRequest()
    {
        fRefreshOperation.Queue();
    }

    void Viewer::OnMonitorChanged(const LWS::MonitorDesc& monitor)
    {
        fCurrentMonitorProperties = monitor;
        fLabelManager.SetContentScale(monitor.contentScale);

        // update the refresh rate.
        fRefreshRateTimes1000 = monitor.displayFrequency == 59 ? 59940 : monitor.displayFrequency * 1000;

        // DPI adjustment. The mouse generates movement events as district units.
        // To keep movement speed constant across several monitors in terms of distance,
        // DPI must be taken care into consideration.
        fDPIadjustmentFactor = {monitor.contentScale.x, monitor.contentScale.y};
    }

    // Update GPU-sized fallback and detail presentations from cached ImageState results.
    // This one-way update creates, updates or releases renderer registrations without
    // processing pixels or drawing a frame.
    void Viewer::UpdateMainRenderImages()
    {
        // Before renderer initialization, retain only CPU data and desired view state.
        if (!fRenderer)
            return;
        Renderer& renderer           = *fRenderer;
        const auto updateRenderImage = [&renderer](std::optional<RenderImage>& presentation,
                                                   const IMCodec::ImageSharedPtr& pixels,
                                                   const RenderImageProperties& properties)
        {
            // Discarded results release their registrations even when the next draw is deferred.
            if (!pixels)
                presentation.reset();
            else if (presentation)
            {
                // Reuse the registration identity. Unchanged pixels keep their upload revision.
                presentation->SetImage(pixels);
                presentation->SetProperties(properties);
            }
            else
                presentation.emplace(renderer.CreateImage(pixels, properties));
        };
        const auto presentation = fSession.fImageState.GetPresentation();
        const auto& fallback    = fSession.fImageState.GetRetainedImage(ImageProcessingStage::Downscaled);
        const bool showFallback = presentation.image == fallback;
        RenderImageProperties properties{
            .position        = presentation.offset,
            .scale           = presentation.scale,
            .opacity         = 1.0,
            .visible         = showFallback,
            .filterType      = fSession.fImageFilter,
            .imageRenderMode = OIV::ImageRenderMode::MainImage,
        };
        // Keep hidden registrations for texture reuse. Their placement is refreshed when selected,
        // so only the visible layer needs its own transform during this update.
        updateRenderImage(fMainRenderImage, fallback, properties);
        properties.visible = !showFallback;
        updateRenderImage(fResampledRenderImage,
                          showFallback ? fSession.fImageState.GetCachedScreenSpaceImage() : presentation.image,
                          properties);
        fRenderViewParams.showGrid = fSession.fIsGridEnabled && presentation.canShowPixelGrid;
    }

    void Viewer::PresentPreparedFrame()
    {
        if (!fPresentationReady)
        {
            fPresentationPending = true;
        }
        else if (!fIsShuttingDown)
        {
            UpdateMainRenderImages();
            fLabelManager.Prepare(*fRenderer);
            fRenderer->SetViewParams(fRenderViewParams);
            fRenderer->Redraw();
        }
    }

    void Viewer::ResumePresentation()
    {
        fPresentationReady = true;
        if (std::exchange(fPresentationPending, false))
            PresentPreparedFrame();
    }

    void Viewer::PerformRefresh()
    {
        using namespace std::chrono;

        const high_resolution_clock::time_point now = high_resolution_clock::now();
        const auto decision = FrameLimiterPolicy::Decide(EnableFrameLimiter, fRefreshTimer.GetEnabled(),
                                                         duration_cast<microseconds>(now - fLastRefreshTime).count(),
                                                         fRefreshRateTimes1000);

        switch (decision.action)
        {
            case FrameRefreshAction::RefreshNow:
                if (EnableFrameLimiter == true)
                    fRefreshTimer.Enable(false);

                PresentPreparedFrame();
                fLastRefreshTime = now;
                break;
            case FrameRefreshAction::ScheduleRefresh:
                fRefreshTimer.SetDueTime(static_cast<uint32_t>(decision.delayMs));
                fRefreshTimer.Enable(true);
                break;
            case FrameRefreshAction::None:
                break;
        }
    }

    void Viewer::OnRefresh()
    {
        PerformRefresh();
    }

    // callback from a too early refresh operation

    void Viewer::OnRefreshTimer()
    {
        using namespace std::chrono;
        PresentPreparedFrame();
        fLastRefreshTime = high_resolution_clock::now();
    }

    void Viewer::OnPreserveSelectionRect()
    {
        LoadImageSpaceSelection();
    }

    LWS::Handle Viewer::GetWindowHandle() const
    {
#ifdef LWS_HAS_WIN32_BACKEND
        return reinterpret_cast<LWS::Handle>(*LWS::Win32::GetHwnd(fWindow.GetWindow()));
#else
        return reinterpret_cast<LWS::Handle>(*LWS::Wayland::GetSurface(fWindow.GetWindow()));
#endif
    }

    void Viewer::UpdateTitle()
    {
        const static LLUtils::native_string_type cachedVersionString = [this]
        {
#if OIV_OFFICIAL_RELEASE == 1
            auto title = LLUtils::native_string_type(LLUTILS_TEXT("OpenImageViewer ")) +
                         LLUtils::StringUtility::ToNativeString(FormatReleaseVersion(CurrentVersion));
    #ifdef OIV_RELEASE_SUFFIX
            title += OIV_RELEASE_SUFFIX;
    #endif
#else
            auto title = LLUtils::native_string_type(LLUTILS_TEXT("OpenImageViewer ")) +
                         LLUtils::StringUtility::ToNativeString(FormatFullVersion(CurrentVersion)) + LLUTILS_TEXT("-") +
                         LLUtils::StringUtility::ToNativeString(std::string(OIV_GIT_SHORT_HASH));

    #if LLUTILS_ARCH_TYPE == LLUTILS_ARCHITECTURE_64
            title += LLUTILS_TEXT(" | 64 bit");
    #else
            title += LLUTILS_TEXT(" | 32 bit");
    #endif
            title += LLUTILS_TEXT(" | ") + MessageHelper::GetFileTime(GetApplicationModulePath());
#endif

#if OIV_OFFICIAL_BUILD == 0
            title += LLUTILS_TEXT(" | UNOFFICIAL");
#endif
            return title;
        }();

        LLUtils::native_string_type title;
        if (fSession.fImageState.GetOpenedImage() != nullptr)
        {
            const ImageSource imageSource = fSession.fImageState.GetOpenedImage()->source;
            if (imageSource == ImageSource::File)
            {
                const auto& committedCurrentFile = fSession.fBrowseSessionController != nullptr
                                                       ? fSession.fBrowseSessionController->GetCommittedCurrentFile()
                                                       : LLUtils::native_string_type{};
                auto decomposedPath              = MessageFormatter::DecomposePath(
                    committedCurrentFile.empty() ? GetOpenedFileName() : committedCurrentFile);
                bool includeIndex   = false;
                size_t displayIndex = 0;
                size_t fileCount    = 0;
                if (GetAppActive() == true && fSession.fBrowseSessionController != nullptr)
                {
                    const auto& fileList = fSession.fBrowseSessionController->GetFolderFileList();
                    if (fileList.IsIndexValid(fileList.GetCurrentIndex()))
                    {
                        includeIndex = true;
                        displayIndex = fileList.GetCurrentIndex() + 1;
                        fileCount    = fileList.GetSize();
                    }
                }

                title = ViewerPresentationPolicy::FormatFileTitlePrefix(decomposedPath.fileName,
                                                                        decomposedPath.extension,
                                                                        decomposedPath.parentPath, includeIndex,
                                                                        displayIndex, fileCount);
            }
            else
                title = ViewerPresentationPolicy::FormatNonFileTitlePrefix(imageSource);
        }
        std::ignore = fWindow.GetWindow().SetTitle(ViewerPresentationPolicy::FormatTitle(title, cachedVersionString));
    }

    void Viewer::OnContextMenuTimer()
    {
        fContextMenuTimer.SetInterval(0);
        const auto pos  = fPlatform.GetMousePosition().value_or(LWS::Point{});
        auto chosenItem = fContextMenu->Show(pos.x - 16, pos.y + -16, AlignmentHorizontal::Center,
                                             AlignmentVertical::Center);
        fMouseInput->Cancel();

        if (chosenItem != nullptr)
        {
            CommandRequestIntenal request;
            request.commandName = chosenItem->userData.command;
            request.args        = chosenItem->userData.args;
            ExecuteCommandInternal(request);
        }
    }

    void Viewer::OnSettingChange(const LLUtils::native_string_type& key, const LLUtils::native_string_type& value)
    {
        ApplySetting(AppSettingsPolicy::ParseAction(key, value));
    }

    void Viewer::ApplySetting(const AppSettingsPolicy::Action& action)
    {
        if (fSession.ApplySetting(action))
            return;
        switch (action.type)
        {
            case AppSettingsPolicy::ActionType::ScreenSpaceDelayMs:
                fScreenSpaceDelayMs = static_cast<uint32_t>(action.integralValue);
                QueueResampling();
                break;
            case AppSettingsPolicy::ActionType::ScreenSpaceWorkFactor:
                fSession.fImageState.SetScreenSpaceWorkFactor(action.floatValue);
                QueueResampling();
                break;
            case AppSettingsPolicy::ActionType::ScreenSpaceZoomTolerance:
                fSession.fImageState.SetScreenSpaceZoomTolerance(action.floatValue);
                QueueResampling();
                break;
            case AppSettingsPolicy::ActionType::ScreenSpacePixelBufferTargetBytes:
                fSession.fImageState.SetScreenSpacePixelBufferTargetBytes(static_cast<uint64_t>(action.integralValue));
                QueueResampling();
                break;
            case AppSettingsPolicy::ActionType::ScreenSpaceTexelThresholdPixels:
                fSession.fImageState.SetScreenSpaceTexelThresholdPixels(action.floatValue);
                QueueResampling();
                break;
            case AppSettingsPolicy::ActionType::SlideshowInterval:
                fSession.fSlideshowPolicy.SetIntervalMs(static_cast<uint32_t>(action.integralValue));
                if (fSession.fSlideshowPolicy.IsEnabled())
                    fTimerSlideShow.SetInterval(fSession.fSlideshowPolicy.GetTimerIntervalMs());
                break;
            case AppSettingsPolicy::ActionType::QuickBrowseDelay:
                fQuickBrowseDelay = static_cast<uint16_t>(action.integralValue);
                break;
            case AppSettingsPolicy::ActionType::AnimationMinFrameInterval:
            case AppSettingsPolicy::ActionType::AnimationSlowFrameInterval:
            case AppSettingsPolicy::ActionType::AnimationSlowSpeedPercent:
            {
                auto settings = fSession.fSequencerPolicy.GetSettings();
                AppSettingsPolicy::StageAnimationSetting(action, settings);
                if (fSession.fSequencerPolicy.SetSettings(settings))
                    UpdateSequencerInterval();
                break;
            }
            case AppSettingsPolicy::ActionType::AutoScrollDeadZoneRadius:
                fAutoScroll->SetDeadZoneRadius(static_cast<int32_t>(action.integralValue));
                break;
            case AppSettingsPolicy::ActionType::AutoScrollSpeedFactorIn:
                fAutoScroll->SetSpeedFactorIn(action.floatValue);
                break;
            case AppSettingsPolicy::ActionType::AutoScrollSpeedFactorOut:
                fAutoScroll->SetSpeedFactorOut(action.floatValue);
                break;
            case AppSettingsPolicy::ActionType::AutoScrollSpeedFactorRange:
                fAutoScroll->SetSpeedFactorRange(static_cast<int32_t>(action.integralValue));
                break;
            case AppSettingsPolicy::ActionType::AutoScrollMaxSpeed:
                fAutoScroll->SetMaxSpeed(static_cast<int32_t>(action.integralValue));
                break;
            case AppSettingsPolicy::ActionType::DeletedFileRemovalMode:
                fDeletedFileRemovalMode = action.deletedFileRemovalMode;
                break;
            case AppSettingsPolicy::ActionType::FileReloadMode:
                fFileReloadPolicy.SetMode(action.fileReloadMode);
                break;
            case AppSettingsPolicy::ActionType::ReloadSettingsFileIfChanged:
                fReloadSettingsFileIfChanged = action.boolValue;
                break;
            case AppSettingsPolicy::ActionType::BackgroundColor:
                fRenderer->SetBackgroundColor(action.backgroundColorIndex,
                                              LLUtils::Color::FromString(
                                                  LLUtils::StringUtility::ToAString(action.textValue)));
                fRefreshOperation.Queue();
                break;
            default:
                break;
        }
    }

    void Viewer::LoadSettings()
    {
        const auto settings = ConfigurationLoader::LoadSettings(fSettingsFile);
        auto animation      = fSession.fSequencerPolicy.GetSettings();
        for (const auto& [key, value] : settings)
        {
            const auto action = AppSettingsPolicy::ParseAction(LLUtils::StringUtility::ToNativeString(key),
                                                               LLUtils::StringUtility::ToNativeString(value));
            if (!AppSettingsPolicy::StageAnimationSetting(action, animation))
                ApplySetting(action);
        }
        // Limits depend on the whole set. Intermediate settings can otherwise clamp a speed
        // that is valid under the final values, and rearm the timer multiple times per reload.
        if (fSession.fSequencerPolicy.SetSettings(animation))
            UpdateSequencerInterval();
    }

    void Viewer::SetUserMessage(const LLUtils::native_string_type& message, GroupID groupID, MessageFlags groupFlags)
    {
        fMessageManager->SetUserMessage(groupID, groupFlags, message);
    }

    void Viewer::ShowImageInfo()
    {
        if (IsImageOpen())
        {
            fSession.RequestColorCount();

            LLUtils::native_string_type imageInfoString = MessageHelper::CreateImageInfoMessage(
                fSession.fImageState.GetOpenedImage(),
                fSession.fImageState.GetRetainedImage(ImageProcessingStage::Source), fDisplayTime,
                fSession.fImageState.GetNumUniqueColors(), fSession.fImageLoader.GetImageCodec());
            TextLabel* imageInfoText = fLabelManager.GetOrCreateTextLabel("imageInfo");

            imageInfoText->SetText(imageInfoString);
            imageInfoText->SetBackgroundColor(LLUtils::Color(0, 0, 0, 127));
            imageInfoText->SetFontSize(12);
            imageInfoText->SetOutlineWidth(2);
            imageInfoText->SetPosition({20, 60});

            if (imageInfoText->IsDirty())
                fRefreshOperation.Queue();
        }
    }

    void Viewer::ShowWelcomeMessage()
    {
        TextLabel* welcomeMessage = fLabelManager.GetOrCreateTextLabel("welcomeMessage");
        welcomeMessage->SetText(
            LLUTILS_TEXT("<textcolor=#4a80e2>Welcome to <textcolor=#ff0000>O<textcolor=#00ff00>I<textcolor=#0000ff>V\n"
                         "<textcolor=#25bc25>Drag <textcolor=#4a80e2>here an image to start\n"
                         "Press <textcolor=#25bc25>F1<textcolor=#4a80e2> to show key bindings"));
        welcomeMessage->SetBackgroundColor(LLUtils::Color(0));
        welcomeMessage->SetOutlineWidth(3);

        UpdateWelcomeMessageLayout();
    }

    void Viewer::UpdateWelcomeMessageLayout()
    {
        using namespace LLUtils;
        if (TextLabel* welcomeMessage = fLabelManager.GetTextLabel("welcomeMessage"); welcomeMessage != nullptr)
        {
            constexpr uint16_t referenceFontSize = 24;
            constexpr uint16_t maximumFontSize   = 44;
            constexpr uint16_t minimumFontSize   = 14;
            constexpr double canvasCoverage      = 0.9;

            const PointI32 dpi{
                static_cast<int32_t>(std::lround(fDPIadjustmentFactor.x * 96.0)),
                static_cast<int32_t>(std::lround(fDPIadjustmentFactor.y * 96.0)),
            };
            // The message and styling are fixed, so cache one moderate-size measurement per DPI and scale its bounds
            // during ordinary window resizes.
            if (fWelcomeMessageReferenceSize == PointI32::Zero || fWelcomeMessageReferenceDPI != dpi)
            {
                welcomeMessage->SetFontSize(referenceFontSize);
                fWelcomeMessageReferenceSize = welcomeMessage->GetMetrics().pixelSize;
                fWelcomeMessageReferenceDPI  = dpi;
            }

            const LWS::PixelSize canvasSize = fWindow.GetCanvasPixelSize();
            const double fitScale           = std::min({
                static_cast<double>(maximumFontSize) / referenceFontSize,
                canvasSize.x * canvasCoverage / fWelcomeMessageReferenceSize.x,
                canvasSize.y * canvasCoverage / fWelcomeMessageReferenceSize.y,
            });
            const auto fontSize             = static_cast<uint16_t>(referenceFontSize * fitScale);
            const bool isVisible            = fontSize >= minimumFontSize;
            welcomeMessage->SetVisible(isVisible);
            if (isVisible)
            {
                welcomeMessage->SetFontSize(fontSize);
                const PointI32 textSize = welcomeMessage->GetMetrics().pixelSize;
                welcomeMessage->SetPosition(
                    static_cast<PointF64>((PointI32{canvasSize.x, canvasSize.y} - textSize) / 2));
            }

            if (welcomeMessage->IsDirty())
                fRefreshOperation.Queue();
        }
    }

    void Viewer::UnloadWelcomeMessage()
    {
        fLabelManager.Remove("welcomeMessage");
    }
}  // namespace OIV

namespace OIV
{
    void Viewer::CloseApplication(bool tray)
    {
        if (fShell.close)
            fShell.close(tray);
    }
    void Viewer::CMD_FileAssociations(const CommandManager::CommandRequest& request,
                                      CommandManager::CommandResult& result)
    {
        if (fShell.fileAssociations)
            fShell.fileAssociations(request, result);
    }
}  // namespace OIV
