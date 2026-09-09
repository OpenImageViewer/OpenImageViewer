#pragma once

#include <Defs.h>
#include <LLUtils/Utility.h>
#include <LLUtils/Rect.h>
#include <LLUtils/EnumClassBitwise.h>

#include <LWS/Clipboard.hpp>
#include <LWS/FileDialog.hpp>
#include <LWS/NotificationIconGroup.hpp>
#include <LWS/Platform.hpp>
#include <LWS/Timer.hpp>

#include "MainWindow.h"
#include "AutoScroll.h"
#include "ImageDescriptor.h"
#include <OIVAppCore/CommandController.h>
#include <OIVShared/AdaptiveMotion.h>
#include <OIVShared/FileSorter.h>
#include <OIVShared/RecursiveDelayOp.h>

#include "ApplicationLog.h"
#include "OIVImage/OIVBaseImage.h"
#include "LabelManager.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"
#include "ContextMenu.h"
#include "ViewerRenderPort.h"
#include <OIVAppCore/AppSettingsPolicy.h>
#include <OIVAppCore/FolderFileList.h>
#include <OIVAppCore/FileReloadPolicy.h>
#include <OIVAppCore/BrowseSessionController.h>
#include <OIVAppCore/FileRemovalPolicy.h>
#include <OIVAppCore/ImageOpenController.h>
#include <OIVAppCore/ImageState.h>
#include <OIVAppCore/OIVImageHelper.h>
#include <OIVAppCore/SelectionRect.h>
#include <OIVAppCore/SlideshowPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <OIVShared/ViewTransformController.h>

#include "UI/MessageManager.h"

#include <NetSettings/GuiProvider.h>
#include <ImageLoader.h>
#include <ImageCodec.h>
#include "InterThreadMessages.h"
#include <OIVShared/ImageResidencyCache.h>

#include <atomic>
#include <any>
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
namespace OIV
{
    struct EventData
    {
        uint16_t id;
        std::any data;
    };

    class ViewerMouseInput;
    struct WindowSizeDecision;
    enum class ImageSizeType
    {
        Original,
        Transformed,
        Visible
    };

    // Determines whether to change zoom / pan when loading a new file
    enum class ResetTransformationMode
    {
        DoNothing = 0,
        ResetAll  = 1,
        Count
    };

    enum class DownscalingTechnique
    {
        None,
        HardwareMipmaps,
        Software,
        Count
    };

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

    class ViewerApplication
    {
      public:

        void OnLabelRefreshRequest();
        explicit ViewerApplication(LWS::PlatformContext& platform);
        ~ViewerApplication();
        void Init(LLUtils::native_string_type filePath, const RendererOptions& rendering = {});
        void Run();
        static LLUtils::native_string_type GetAppDataFolder();
        static LWS::Handle FindTrayBarWindow();

      private:  // types

        friend class ViewerMouseInput;

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

        LLUtils::native_string_type GetLogFilePath();
        void HandleException(bool isFromLibrary, LLUtils::Exception::EventArgs args,
                             LLUtils::native_string_type seperatedCallStack);
#pragma region platform event handling
        bool handleKeyInput(const LWS::AnyEvent& eventData);
        std::intptr_t ClientWindwMessage(const LWS::AnyEvent& eventData);
        void SetTopMostUserMesage();
        void ProcessTopMost();
        void SetAppActive(bool active);
        bool GetAppActive() const;

        bool HandleWinMessageEvent(const LWS::AnyEvent& eventData);
        void CloseApplication(bool closeToTray);
        bool HandleFileDragDropEvent(const LWS::EventDragDropFile& eventDragDropFile);
        bool HandleMessages(const LWS::AnyEvent& eventData);
        bool HandleClientWindowMessages(const LWS::AnyEvent& eventData);
        double GetMinimumPixelSize();

#pragma endregion platform event handling
        void AddCommandsAndKeyBindings();
        void AddPlatformKeyBindings();
        void OnMonitorChanged(const EventManager::MonitorChangeEventParams& params);
        void PerformRefresh();
        void SetUserMessage(const LLUtils::native_string_type& message, GroupID groupID = 0,
                            MessageFlags groupFlags = MessageFlags::Interchangeable);
        bool ExecuteCommandInternal(const CommandRequestIntenal& request);
        bool ExecuteCommand(const CommandManager::CommandRequest& request);
        bool ExecutePredefinedCommand(std::string command);
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
        void OnPreserveSelectionRect();
        LWS::Handle GetWindowHandle() const;
        void UpdateTitle();
        // bool JumpTo(FileIndexType fileIndex);
        bool JumpFiles(FolderFileList::index_type step);
        void ToggleFullScreen(bool multiFullScreen);
        void ToggleBorders();
        void SetSlideShowEnabled(bool enabled);
        bool GetSlideShowEnabled() const { return fSlideshowPolicy.IsEnabled(); }
        void SetFilterLevel(OIV_Filter_type filterType);
        OIV_Filter_type GetFilterType() const;
        void ToggleGrid();
        void UpdateRenderViewParams();
        void Pan(const LLUtils::PointF64& panAmount);
        void Zoom(double precentage, int zoomX = -1, int zoomY = -1);
        void ZoomInternal(double amount, int zoomX, int zoomY);
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
        LLUtils::PointF64 ResolveOffset(const LLUtils::PointF64& point);
        void SetOffset(LLUtils::PointF64 offset, bool preserveOffsetLockState = false);
        void SetOriginalSize();
        void OnScroll(const LLUtils::PointF64& panAmount);
        void OnImageSelectionChanged(const ImageList::ImageSelectionChangeArgs& ImageSelectionChangeArgs);
        bool LoadFile(LLUtils::native_string_type filePath, IMCodec::PluginTraverseMode loaderFlags);
        bool LoadFileOrFolder(const LLUtils::native_string_type& filePath, IMCodec::PluginTraverseMode traverseMode);
        bool ProcessImageLoadResult(const ImageLoadResult& loadResult);
        void LoadOivImage(OIVBaseImageSharedPtr oivImage);
        void UpdateOpenImageUI();
        void UnloadWelcomeMessage();
        void ShowWelcomeMessage();
        const LLUtils::native_string_type& GetOpenedFileName() const;
        bool IsImageOpen() const;
        bool IsOpenedImageIsAFile() const;
        void TransformImage(IMUtil::AxisAlignedRotation transform, IMUtil::AxisAlignedFlip flip);
        void LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height, uint32_t rowPitch,
                     IMCodec::TexelFormat texelFormat);
        ClipboardDataType PasteFromClipBoard();
        void LoadClipboardText(const LLUtils::native_string_type& text);
        bool SetClipboardImage(IMCodec::ImageSharedPtr image);
        OperationResult CropVisibleImage();
        OperationResult CopyVisibleToClipBoard();
        OperationResult CutSelectedArea();
        void AfterFirstFrameDisplayed();
        void UnloadOpenedImaged();
        void DeleteOpenedFile(bool permanently);
        void RefreshImage();
        void DisplayOpenedFileName();
        void UpdateExposure();
        bool ToggleColorCorrection();
        void CancelSelection();
        void LoadSubImages();
        void AddImageToControl(IMCodec::ImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages);
        void OnContextMenuTimer();
        void SetDownScalingTechnique(DownscalingTechnique technique);
        bool IsMainThread() const { return fMainThreadID == std::this_thread::get_id(); }
        void OnFileChangedImpl(const IFileWatcher::FileChangedEventArgs*
                                   fileChangedEventArgs);  // file change handler, runs in the main thread.
        void OnFileChanged(IFileWatcher::FileChangedEventArgs fileChangedEventArgs);  // callback from file watcher
        void OnFileIndexResidencyReady(const LLUtils::native_string_type& fileName, IMCodec::ImageSharedPtr image);

        void ProcessCurrentFileChanged();
        void ProcessRemovalOfOpenedFile(const LLUtils::native_string_type& fileName);

        void OnNotificationIcon(LWS::NotificationIconGroup::NotificationIconEventArgs args);
        void DelayResamplingCallback();
        void ShowImageInfo();
        void CountColorsAsync();
        void SetImageInfoVisible(bool visible);
        bool GetImageInfoVisible() const;
        void PerformReloadFile(const LLUtils::native_string_type& requestedFile);
        void HandleReloadAction(ReloadAction action, const LLUtils::native_string_type& requestedFile);
        void ShowSettings();
        static void NetSettingsCallback_(ItemChangedArgs* callback);
        void NetSettingsCallback(ItemChangedArgs* callback);
        IMCodec::ImageSharedPtr GetImageByIndex(int32_t index);
        bool IsSubImagesVisible() const;
        void UpdateSelectionRectText();
        void OnImageReady(IMCodec::ImageSharedPtr image);
        LLUtils::PointI32 SnapToScreenSpaceImagePixels(LLUtils::PointI32 pointOnScreen);
        void OnMessageFromBackgroundThread(const EventData& sharedData);
        void OnCountingColorsCompleted(const CountColorsData& countColorsData);
        bool HandleEventCallback(const std::function<bool()>& callback) noexcept;
        std::function<void()> MakeSafeCallback(std::function<void()> callback);
        void InitializePlatformState();
        void InitializeRawInput();
        [[nodiscard]] int GetRawNavigationDirection() const;
        void InitializeNotificationIcons();
        void InitializeRenderer(const RendererOptions& rendering);
        [[nodiscard]] WindowSizeDecision GetWindowSizeDecision(const CommandManager::CommandArgs& args) const;
        [[nodiscard]] LWS::Rect GetNotificationIconRect(LWS::NotificationIconGroup::IconID iconId) const;
        [[nodiscard]] static LLUtils::native_string_type GetApplicationModulePath();

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

      private:  // member fields

        LWS::PlatformContext& fPlatform;

#pragma region FrameLimiter
        const bool EnableFrameLimiter = true;
        std::chrono::high_resolution_clock::time_point fLastRefreshTime;
        LWS::HighPrecisionTimer fRefreshTimer;
        uint32_t fRefreshRateTimes1000 = 60'000;
        LWS::MonitorDesc fCurrentMonitorProperties{};
        MonitorProvider fMonitorProvider;
#pragma endregion FrameLimiter
        MainWindow fWindow;
        // Images release before the renderer, and the native canvas outlives both.
        std::unique_ptr<IViewerRenderPort> fRenderGateway;
        AutoScrollUniquePtr fAutoScroll;
        RecursiveDelayedOp fRefreshOperation;
        RecursiveDelayedOp fPreserveImageSpaceSelection;
        double fMaxPixelSize = 30.0;
        double fMinImageSize = 150.0;
        SlideshowPolicy fSlideshowPolicy;
        bool fReloadSettingsFileIfChanged             = false;
        IFileWatcher::FolderID fCOnfigurationFolderID = 0;
        int fCurrentFrame                             = 0;
        double fCurrentSequencerSpeed                 = 1.0;
        OIVBaseImageSharedPtr fCountingImageColor;
        std::atomic_bool fIsColorThreadRunning = false;
        std::thread fCountingColorsThread;
        // FileCache fFileCache;

        struct RawInputState;
        struct NativeWindowState;
        struct RawInputStateDeleter
        {
            void operator()(RawInputState* state) const noexcept;
        };
        struct NativeWindowStateDeleter
        {
            void operator()(NativeWindowState* state) const noexcept;
        };
        std::unique_ptr<RawInputState, RawInputStateDeleter> fRawInputState;
        std::unique_ptr<NativeWindowState, NativeWindowStateDeleter> fNativeWindowState;

        bool fIsGridEnabled                         = false;
        OIV_PROP_TransparencyMode fTransparencyMode = OIV_PROP_TransparencyMode::TM_Medium;
        OIVBaseImageSharedPtr fAutoScrollAnchor;
        std::thread::id fMainThreadID = std::this_thread::get_id();
        SelectionRect fSelectionRect;
        uint32_t fQueueResamplingDelay        = 50;
        LLUtils::RectI32 fImageSpaceSelection = LLUtils::RectI32::Zero;
        LWS::Timer fTimerTopMostRetention;
        LWS::Timer fTimerSlideShow;
        LWS::Clipboard fClipboardHelper;

        int fTopMostCounter = 0;
        LWS::Timer fTimerNoActiveZoom;
        LWS::Timer fTimerNavigation;
        bool fIsResamplingEnabled          = false;
        bool fQueueImageInfoLoad           = false;
        uint16_t fQuickBrowseDelay         = 100;
        bool fDisplayBiggestSubImageOnLoad = true;

        /// determines whether the current loaded file is the initial file being loaded at startup
        bool fIsTryToLoadInitialFile = false;
        bool fIsFirstFrameDisplayed  = false;
        bool fIsOffsetLocked         = false;
        bool fIsLockFitToScreen      = false;
        bool fShowBorders            = true;
        bool fImageInfoVisible       = false;
        bool fIsActive               = false;
        LLUtils::PointF64 fDPIadjustmentFactor{1.0, 1.0};
        IMCodec::ImageLoader fImageLoader;
        std::unique_ptr<ImageOpenController> fImageOpenController;
        // LWS::ClipboardFormatType fRTFFormatID {};
        // LWS::ClipboardFormatType fHTMLFormatID {};

        DeletedFileRemovalMode fDeletedFileRemovalMode = DeletedFileRemovalMode::DeletedInternally;

        LLUtils::native_string_type fRequestedFileForRemoval;
        FileReloadPolicy fFileReloadPolicy;
        LLUtils::PointF64 fImageMargins{0.75, 0.75};
        LLUtils::native_string_type DefaultTextKeyColorTag   = LLUTILS_TEXT("<textcolor=#ff8930ff>");
        LLUtils::native_string_type DefaultTextValueColorTag = LLUTILS_TEXT("<textcolor=#7672ffff>");
        LLUtils::StopWatch fFileDisplayTimer;
        ResetTransformationMode fResetTransformationMode           = ResetTransformationMode::ResetAll;
        const OIV_CMD_ColorExposure_Request DefaultColorCorrection = {1.0, 0.0, 1.0, 1.0, 1.0};
        OIV_CMD_ColorExposure_Request fColorExposure               = DefaultColorCorrection;
        OIV_CMD_ColorExposure_Request fLastColorExposure           = fColorExposure;
        VirtualStatusBar fVirtualStatusBar;

        AdaptiveMotion fAdaptiveZoom         = AdaptiveMotion(1.0, 0.6, 1.0);
        AdaptiveMotion fAdaptivePanLeftRight = AdaptiveMotion(1.6, 1.0, 5.2);
        AdaptiveMotion fAdaptivePanUpDown    = AdaptiveMotion(1.6, 1.0, 5.2);
        ImageState fImageState;

        CommandController fCommandController;
        std::unique_ptr<FreeType::FreeTypeConnector> fFreeType;
        LabelManager fLabelManager;
        KeyDoubleTap fDoubleTap;
        DownscalingTechnique fDownScalingTechnique = DownscalingTechnique::Software;
        LLUtils::native_string_type fCurrentFolderWatched;
        std::set<LLUtils::native_string_type> fKnownFileTypesSet;
        LLUtils::native_string_type fKnownFileTypes;
        LWS::FileDialogFilterBuilder fOpenComDlgFilters;
        LWS::FileDialogFilterBuilder fSaveComDlgFilters;
        LLUtils::native_string_type fDefaultSaveFileExtension = LLUTILS_TEXT("png");
        int16_t fDefaultSaveFileFormatIndex                   = -1;
        LLUtils::native_string_type fPendingFolderLoad;
        LLUtils::StopWatch fLastImageLoadTimeStamp;
        LWS::NotificationIconGroup::IconID fNotificationIconID;
        std::unique_ptr<MessageManager> fMessageManager;
        void OnSettingChange(const LLUtils::native_string_type& key, const LLUtils::native_string_type& value);
        void LoadSettings();
        void SetResamplingEnabled(bool enable);
        bool GetResamplingEnabled() const;
        void QueueResampling();
        void SortFolderFileList();
        void ApplyBrowseSessionResult(const BrowseSessionController::BrowseSessionResult& result);
        void DrainUiCompletions();

        template <typename T>
        void QueueUiCompletion(uint16_t id, T&& value)
        {
            std::unique_lock lock(fUiCompletionMutex);
            const bool scheduleDrain = fUiCompletions.empty();
            fUiCompletions.push_back(EventData{id, std::forward<T>(value)});
            if (scheduleDrain)
            {
                auto drain = [this, lifetime = std::weak_ptr(fUiLifetime)]
                {
                    if (lifetime.lock() != nullptr)
                        DrainUiCompletions();
                };
                // Let an immediately awakened UI thread drain without waiting on the producer.
                lock.unlock();
                std::ignore = fPlatform.PostTask(std::move(drain));
            }
        }

        std::unique_ptr<ContextMenu<int>> fNotificationContextMenu;

        ApplicationLog mLogFile{GetLogFilePath(), true};
        // Disconnect after workers stop and before the log is destroyed.
        LLUtils::Exception::OnExceptionEventType::Connection fExceptionConnection;

        struct MenuItemData
        {
            std::string command;
            std::string args;
        };

        std::unique_ptr<ContextMenu<MenuItemData>> fContextMenu;
        std::unique_ptr<ViewerMouseInput> fMouseInput;
        LWS::Timer fContextMenuTimer;
        LWS::Timer fSequencerTimer;
        FileSorter fFileSorter;
        struct UiLifetime
        {
        };
        std::shared_ptr<UiLifetime> fUiLifetime{std::make_shared<UiLifetime>()};
        std::mutex fUiCompletionMutex;
        std::vector<EventData> fUiCompletions;
        std::atomic_bool fIsShuttingDown = false;
        ImageResidencyCache fImageResidencyCache;
        std::unique_ptr<IFileWatcher> fFileWatcher;
        std::unique_ptr<BrowseSessionController> fBrowseSessionController;
        LWS::EventConnection fWindowConnection;
        LWS::EventConnection fCanvasConnection;
        LWS::EventConnection fPlatformConnection;
    };
}  // namespace OIV
