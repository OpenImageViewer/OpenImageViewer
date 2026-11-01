#pragma once

#include <RendererFactory.h>
#include <OIVAppCore/UserStorage.h>
#include <LLUtils/Utility.h>
#include <LLUtils/Exception.h>
#include <LLUtils/Rect.h>
#include <LLUtils/EnumClassBitwise.h>

#include <LWS/Clipboard.hpp>
#include <LWS/FileDialog.hpp>
#include <LWS/Platform.hpp>
#include <LWS/Timer.hpp>

#include "MainWindow.h"
#include "AutoScroll.h"
#include "ImageDescriptor.h"
#include <OIVAppCore/CommandController.h>
#include <OIVAppCore/ConfigurationLoader.h>
#include <OIVShared/AdaptiveMotion.h>
#include <OIVShared/FileSorter.h>
#include <OIVShared/RecursiveDelayOp.h>

#include <LLUtils/Logging/Logger.h>
#include <RendererCore/Renderer.h>
#include "LabelManager.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"
#include "ContextMenu.h"
#include <OIVAppCore/AppSettingsPolicy.h>
#include <OIVAppCore/SequencerPolicy.h>
#include <OIVAppCore/FolderFileList.h>
#include <OIVAppCore/FileReloadPolicy.h>
#include <OIVAppCore/FileAssociationPolicy.h>
#include <OIVAppCore/BrowseSessionController.h>
#include <OIVAppCore/FileRemovalPolicy.h>
#include <OIVAppCore/ImageOpenController.h>
#include <OIVAppCore/ViewerSession.h>
#include <OIVAppCore/SelectionRect.h>
#include <OIVAppCore/SlideshowPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <OIVShared/ViewTransformController.h>

#include "UI/MessageManager.h"

#include "ViewerShell.h"
#include "ViewerResources.h"
#include <ImageLoader.h>
#include <ImageCodec.h>
#include <OIVAppCore/ViewerCompletion.h>
#include <OIVAppCore/ImageResidencyCache.h>

#include <atomic>
#include <variant>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <vector>
namespace OIV::Win32
{
    class HostedInputAdapter;
}

namespace OIV
{
    class ViewerMouseInput;
    struct WindowSizeDecision;
    enum class ClipboardDataType
    {
        None,
        Image,
        Text
    };

    enum class UserMessageGroups
    {
        Default,
        SuccessfulFileLoad,
        FailedFileLoad,
        WindowOnTop
    };

    // Assue Count exists and presenting the total number of values in an enum.
    template <typename T, typename UnderlyingType = typename std::underlying_type_t<T>>
    T GetNextEnumValue(T enumVal)
    {
        //   using UnderlyingType = std::underlying_type_t<T>;
        return static_cast<T>((static_cast<UnderlyingType>(enumVal) + static_cast<UnderlyingType>(1)) %
                              static_cast<UnderlyingType>(T::Count));
    }

    class KeyDoubleTap
    {
        static constexpr int MaxDelayBetweenTaps = 320;
        std::chrono::high_resolution_clock::time_point fLastTap;

      public:

        std::function<void()> callback;
        void SetState(bool down)
        {
            const bool up = !down;
            using namespace std::chrono;

            if (up)
            {
                high_resolution_clock::time_point now = high_resolution_clock::now();
                if (std::chrono::duration_cast<milliseconds>(now - fLastTap).count() < MaxDelayBetweenTaps)
                {
                    // trigger double tap.
                    callback();
                    fLastTap = high_resolution_clock::time_point::min();
                }
                fLastTap = high_resolution_clock::now();
            }
        }
    };

    // Owns the current native presentation, borrowing a longer-lived session and frontend services. Surface
    // destruction invalidates UI callbacks before GPU cleanup and leaves CPU state and session producers intact.
    class Viewer
    {
      public:

        void OnLabelRefreshRequest();
        // The storage session and its immutable directories must outlive the viewer and its workers.
        Viewer(LWS::PlatformContext& platform, ViewerSession& session, const UserDirectories& directories,
               ViewerResources resources, ViewerShell shell);
        ~Viewer();
        void Init(LLUtils::native_string_type filePath, const RendererOptions& rendering,
                  const LWS::WindowConfig& windowConfig);
        bool IsImageOpen() const;
        bool LoadFile(LLUtils::native_string_type filePath, IMCodec::PluginTraverseMode loaderFlags);
        bool ExecutePredefinedCommand(std::string command);
        OperationResult CopyVisibleToClipBoard();
        void Stop();
        // Standalone defers nonessential setup until paint. A host can explicitly finish it before ListLoad returns.
        void CompleteInitialization();
        // Borrowed composite for native host parenting, resize, and focus. The viewer retains ownership.
        MainWindow& GetWindow() { return fWindow; }
        bool IsClosing() const { return fIsShuttingDown; }
        enum class ViewFit
        {
            Original,
            Fit,
            FitLargerOnly
        };
        // Host presentation requests reuse the normal viewer fit and pan behavior.
        void SetViewFit(ViewFit fit, bool center);
        bool ScrollToPercent(int percent);

      private:  // types

        friend class ViewerMouseInput;
        friend class Win32::HostedInputAdapter;
        friend class ViewerApplication;

        static constexpr LWS::WindowStyle WindowChromeStyles = LWS::WindowStyle::Caption |
                                                               LWS::WindowStyle::CloseButton |
                                                               LWS::WindowStyle::ResizableBorder |
                                                               LWS::WindowStyle::MinimizeButton |
                                                               LWS::WindowStyle::MaximizeButton;

        struct CommandRequestIntenal
        {
            std::string commandName;
            std::string args;
        };

      private:  // methods

#pragma region platform event handling
        bool handleKeyInput(const LWS::AnyEvent& eventData);
        std::intptr_t ClientWindwMessage(const LWS::AnyEvent& eventData);
        void SetAppActive(bool active);
        bool GetAppActive() const;

        void CloseApplication(bool closeToTray);
        void CMD_FileAssociations(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        bool HandleWinMessageEvent(const LWS::AnyEvent& eventData);
        bool HandleFileDragDropEvent(const LWS::EventDragDropFile& eventDragDropFile);
        bool HandleMessages(const LWS::AnyEvent& eventData);
        bool HandleClientWindowMessages(const LWS::AnyEvent& eventData);
        double GetMinimumPixelSize();

#pragma endregion platform event handling
        void AddCommandsAndKeyBindings();
        void AddPlatformKeyBindings();
        void OnMonitorChanged(const LWS::MonitorDesc& monitor);
        void PerformRefresh();
        void UpdateMainRenderImages();
        void PresentPreparedFrame();
        void ResumePresentation();
        static LLUtils::native_string_type DescribeImage(const IMCodec::ImageSharedPtr& pixels);
        void SetUserMessage(const LLUtils::native_string_type& message, GroupID groupID = 0,
                            MessageFlags groupFlags = MessageFlags::Interchangeable);
        bool ExecuteCommandInternal(const CommandRequestIntenal& request);
        bool ExecuteCommand(const CommandManager::CommandRequest& request);
        void PostInitOperations();
#pragma region Commands
        void CMD_Zoom(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ViewState(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ToggleKeyBindings(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ShowSystemInfo(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_OpenFile(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_AxisAlignedTransform(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ToggleColorCorrection(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ColorCorrection(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_Pan(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_Placement(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_CopyToClipboard(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_PasteFromClipboard([[maybe_unused]] const CommandManager::CommandRequest& request,
                                    CommandManager::CommandResult& result);
        void CMD_ImageManipulation(const CommandManager::CommandRequest& request,
                                   CommandManager::CommandResult& result);
        void CMD_Navigate(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_Shell(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_DeleteFile(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_SetWindowSize(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_SortFiles(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_Sequencer(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);

#pragma endregion  // Commands
        void OnSelectionRectChanged(const LLUtils::RectI32&, bool);
        void OnRefresh();
        void OnRefreshTimer();
        void UpdateSequencerInterval();
        void OnPreserveSelectionRect();
        LWS::Handle GetWindowHandle() const;
        void UpdateTitle();
        // bool JumpTo(FileIndexType fileIndex);
        bool JumpFiles(FolderFileList::index_type step);
        void ToggleFullScreen(bool multiFullScreen);
        void ToggleBorders();
        void SetSlideShowEnabled(bool enabled);
        bool GetSlideShowEnabled() const { return fSession.fSlideshowPolicy.IsEnabled(); }
        void SetFilterLevel(OIV::FilterType filterType);
        OIV::FilterType GetFilterType() const;
        void ToggleGrid();
        void UpdateRenderViewParams();
        void Pan(const LLUtils::PointF64& panAmount);
        // One wheel step is one logical detent. Fractional steps are supported.
        void Zoom(double wheelSteps, int zoomX = -1, int zoomY = -1);
        void ZoomInternal(double steps, int zoomX, int zoomY);
        void FitToClientAreaAndCenter();
        LLUtils::PointF64 GetImageSize(ImageSizeType type);
        void SetImageSpaceSelection(const LLUtils::RectI32& rect);
        void SaveImageSpaceSelection();
        void LoadImageSpaceSelection();
        void SetZoomInternal(double zoom, int x = -1, int y = -1, bool preserveFitToScreenState = false);
        double GetScale() const;
        LLUtils::PointF64 GetOffset() const;
        LLUtils::PointF64 ImageToClient(LLUtils::PointF64 imagepos) const;
        LLUtils::RectF64 ImageToClient(LLUtils::RectF64 clientRect) const;
        LLUtils::PointF64 ClientToImage(LLUtils::PointI32 clientPos) const;
        LLUtils::RectF64 ClientToImage(LLUtils::RectI32 clientRect) const;
        LLUtils::RectI32 ClientToImageRounded(LLUtils::RectI32 clientRect) const;
        LLUtils::PointF64 GetCanvasCenter();
        void UpdateTexelPos();
        void AutoPlaceImage(bool forceCenter = false);
        void UpdateWindowSize();
        void Center();
        void SetOffset(LLUtils::PointF64 offset, bool preserveOffsetLockState = false);
        void SetOriginalSize();
        void OnScroll(const LLUtils::PointF64& panAmount);
        void OnImageSelectionChanged(const ImageList::ImageSelectionChangeArgs& ImageSelectionChangeArgs);
        bool LoadFileOrFolder(const LLUtils::native_string_type& filePath, IMCodec::PluginTraverseMode traverseMode);
        bool ProcessImageLoadResult(const ImageLoadResult& loadResult);
        void LoadOivImage(ViewerImage oivImage);
        void UpdateOpenImageUI();
        void UnloadWelcomeMessage();
        void ShowWelcomeMessage();
        void UpdateWelcomeMessageLayout();
        const LLUtils::native_string_type& GetOpenedFileName() const;
        bool IsOpenedImageIsAFile() const;
        void TransformImage(IMUtil::AxisAlignedRotation transform, IMUtil::AxisAlignedFlip flip);
        void LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height, uint32_t rowPitch,
                     IMCodec::TexelFormat texelFormat);
        ClipboardDataType PasteFromClipBoard();
        void LoadClipboardText(const LLUtils::native_string_type& text);
        bool SetClipboardImage(IMCodec::ImageSharedPtr image);
        OperationResult CropVisibleImage();
        OperationResult CutSelectedArea();
        void UnloadOpenedImaged();
        void DeleteOpenedFile(bool permanently);
        void RefreshImage();
        void DisplayOpenedFileName();
        void UpdateExposure();
        bool ToggleColorCorrection();
        void CancelSelection();
        void UpdateSelectionCursor(bool altPressed);
        void LoadSubImages(bool restoreSelection = false);
        void AddImageToControl(IMCodec::ImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages);
        void OnContextMenuTimer();
        void SetDownScalingTechnique(DownscalingTechnique technique);
        bool IsMainThread() const { return fMainThreadID == std::this_thread::get_id(); }
        void OnFileChangedImpl(const IFileWatcher::FileChangedEventArgs*
                                   fileChangedEventArgs);  // file change handler, runs in the main thread.
        void OnFileIndexResidencyReady(const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image);

        void ProcessCurrentFileChanged();
        void ProcessRemovalOfOpenedFile(const LLUtils::native_string_type& fileName);

        void DelayResamplingCallback();
        void ShowImageInfo();
        void SetImageInfoVisible(bool visible);
        bool GetImageInfoVisible() const;
        void PerformReloadFile(const LLUtils::native_string_type& requestedFile);
        void HandleReloadAction(ReloadAction action, const LLUtils::native_string_type& requestedFile);
        bool ConfirmFileReload(const LLUtils::native_string_type& requestedFile);
        IMCodec::ImageSharedPtr GetImageByIndex(int32_t index);
        bool IsSubImagesVisible() const;
        void UpdateSelectionRectText();
        LLUtils::PointI32 SnapToScreenSpaceImagePixels(LLUtils::PointI32 pointOnScreen);
        void OnMessageFromBackgroundThread(const ViewerCompletion& sharedData);
        void OnCountingColorsCompleted(const CountColorsData& countColorsData);
        bool HandleEventCallback(const std::function<bool()>& callback) noexcept;
        std::function<void()> MakeSafeCallback(std::function<void()> callback);
        void InitializePlatformState();
        void InitializeRawInput();
#ifdef LWS_HAS_WIN32_BACKEND
        void EnableRawMouse();
#endif
        [[nodiscard]] int GetRawNavigationDirection() const;
        void InitializeRenderer(const RendererOptions& rendering);
        [[nodiscard]] WindowSizeDecision GetWindowSizeDecision(const CommandManager::CommandArgs& args) const;
        [[nodiscard]] LLUtils::native_string_type GetApplicationModulePath() const { return fModulePath.native(); }

      private:  // member fields

        ViewerShell fShell;
        LWS::PlatformContext& fPlatform;
        ViewerSession& fSession;
        // Input acceleration and idle refinement are reset when a new surface attaches.
        bool fIsResamplingEnabled                      = false;
        static constexpr double ZoomAmountPerWheelStep = 0.2;
        AdaptiveMotion fAdaptiveZoom{ZoomAmountPerWheelStep, 0.6, 1.0};
        AdaptiveMotion fAdaptivePanLeftRight = AdaptiveMotion(1.6, 1.0, 5.2);
        AdaptiveMotion fAdaptivePanUpDown    = AdaptiveMotion(1.6, 1.0, 5.2);

        const std::filesystem::path fResources;
        const std::filesystem::path fSettingsFile;
        const std::filesystem::path fModulePath;
        const UserDirectories& fUserDirectories;

#pragma region FrameLimiter
        const bool EnableFrameLimiter = true;
        std::chrono::high_resolution_clock::time_point fLastRefreshTime;
        LWS::HighPrecisionTimer fRefreshTimer;
        uint32_t fRefreshRateTimes1000 = 60'000;
        LWS::MonitorDesc fCurrentMonitorProperties{};
        MonitorProvider fMonitorProvider;
#pragma endregion FrameLimiter
        MainWindow fWindow;
        // Native rendering stops before window teardown. Presentations release before their renderer owner.
        // Declared before every presentation owner so registrations are destroyed first.
        std::unique_ptr<Renderer> fRenderer;
        RendererType fRendererType = RendererType::Null;
        ViewParameters fRenderViewParams{};
        std::optional<LWS::PixelSize> fViewportSize;
        // Optional main-image registrations, never created for CPU-only stages.
        std::optional<RenderImage> fMainRenderImage;
        // Retained software-resampled or viewport-detail registration, released with its CPU pixels.
        std::optional<RenderImage> fResampledRenderImage;
        // Time from loading to prepared display, formatted by viewer information policy.
        double fDisplayTime = 0.0;
        // Deferred platform readiness and coalesced redraw request belong to the scheduler.
        bool fPresentationReady = true;
        // A redraw requested before platform presentation becomes ready.
        bool fPresentationPending = false;
        AutoScrollUniquePtr fAutoScroll;
        RecursiveDelayedOp fRefreshOperation;
        RecursiveDelayedOp fPreserveImageSpaceSelection;
        bool fReloadSettingsFileIfChanged = false;

        struct RawInputState;
        struct RawInputStateDeleter
        {
            void operator()(RawInputState* state) const noexcept;
        };
        std::unique_ptr<RawInputState, RawInputStateDeleter> fRawInputState;

        // Independent autoscroll overlay released before renderer shutdown.
        std::optional<RenderImage> fAutoScrollAnchor;
        std::thread::id fMainThreadID = std::this_thread::get_id();
        SelectionRect fSelectionRect;
        uint32_t fQueueResamplingDelay = 50;
        uint32_t fScreenSpaceDelayMs   = 100;
        LWS::Timer fTimerSlideShow;
        LWS::Clipboard fClipboardHelper;

        LWS::Timer fTimerNoActiveZoom;
        LWS::Timer fTimerNavigation;
        bool fQueueImageInfoLoad   = false;
        uint16_t fQuickBrowseDelay = 100;

        /// determines whether the current loaded file is the initial file being loaded at startup
        bool fIsTryToLoadInitialFile = false;
        bool fIsFirstFrameDisplayed  = false;
        bool fRestoringSession{};
        bool fSessionAttached{};
        bool fShowBorders      = true;
        bool fImageInfoVisible = false;
        bool fIsActive         = false;
        LLUtils::PointF64 fDPIadjustmentFactor{1.0, 1.0};
        LLUtils::PointI32 fWelcomeMessageReferenceSize{};
        LLUtils::PointI32 fWelcomeMessageReferenceDPI{};
        // LWS::ClipboardFormatType fRTFFormatID {};
        // LWS::ClipboardFormatType fHTMLFormatID {};

        DeletedFileRemovalMode fDeletedFileRemovalMode = DeletedFileRemovalMode::DeletedInternally;

        LLUtils::native_string_type fRequestedFileForRemoval;
        FileReloadPolicy fFileReloadPolicy;
        LLUtils::native_string_type DefaultTextKeyColorTag   = LLUTILS_TEXT("<textcolor=#ff8930ff>");
        LLUtils::native_string_type DefaultTextValueColorTag = LLUTILS_TEXT("<textcolor=#7672ffff>");
        LLUtils::StopWatch fFileDisplayTimer;
        VirtualStatusBar fVirtualStatusBar;

        ConfigurationLoader::CommandGroupList fConfiguredCommands;
        ConfigurationLoader::KeyBindingList fConfiguredBindings;
        CommandController fCommandController;
        std::unique_ptr<FreeType::FreeTypeConnector> fFreeType;
        LabelManager fLabelManager;
        KeyDoubleTap fDoubleTap;
        LLUtils::native_string_type fCurrentFolderWatched;
        LWS::ListFileDialogFilters fOpenComDlgFilters;
        LWS::ListFileDialogFilters fSaveComDlgFilters;
        LLUtils::native_string_type fDefaultSaveFileExtension = LLUTILS_TEXT("png");
        int16_t fDefaultSaveFileFormatIndex                   = -1;
        LLUtils::StopWatch fLastImageLoadTimeStamp;
        std::unique_ptr<MessageManager> fMessageManager;
        void OnSettingChange(const LLUtils::native_string_type& key, const LLUtils::native_string_type& value);
        void ApplySetting(const AppSettingsPolicy::Action& action);
        void LoadSettings();
        void SetResamplingEnabled(bool enable);
        bool GetResamplingEnabled() const;
        void QueueResampling();
        void SortFolderFileList();
        void ApplyBrowseSessionResult(const BrowseSessionController::BrowseSessionResult& result);
        void DrainUiCompletions();

        template <typename T>
        void QueueUiCompletion(T&& value)
        {
            fSession.QueueCompletion(std::forward<T>(value));
        }

        MonitorProvider::ChangeEvent::Subscription fMonitorSubscription;
        ImageList::ImageSelectionChangEvent::Subscription fImageSelectionSubscription;

        struct MenuItemData
        {
            std::string command;
            std::string args;
        };

        std::unique_ptr<ContextMenu<MenuItemData>> fContextMenu;
        std::unique_ptr<ViewerMouseInput> fMouseInput;
        LWS::Timer fContextMenuTimer;
        LWS::Timer fSequencerTimer;
        struct UiLifetime
        {
        };
        std::shared_ptr<UiLifetime> fUiLifetime{std::make_shared<UiLifetime>()};
        const std::weak_ptr<UiLifetime> fUiWeakLifetime{fUiLifetime};
        std::atomic_bool fIsShuttingDown = false;
        LWS::EventConnection fWindowConnection;
        LWS::EventConnection fCanvasConnection;
        LWS::EventConnection fPlatformConnection;
    };
}  // namespace OIV
