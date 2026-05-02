#pragma once

#include <Defs.h>
#include <LLUtils/Utility.h>
#include <LLUtils/Logging/LogPredefined.h>
#include <LLUtils/Rect.h>
#include <LLUtils/EnumClassBitwise.h>

#include <Win32/MonitorInfo.h>
#include <Win32/NotificationIconGroup.h>
#include <Win32/HighPrecisionTimer.h>
#include <Win32/Timer.h>
#include <Win32/Win32Window.h>
#include <Win32/Clipboard.h>
#include <Win32/FileDialog.h>

#include "win32/MainWindow.h"
#include "AutoScroll.h"
#include "ImageDescriptor.h"
#include <OIVAppCore/CommandController.h>
#include <OIVShared/AdaptiveMotion.h>
#include <OIVShared/FileSorter.h>
#include <OIVShared/RecursiveDelayOp.h>

#include <LInput/Keys/KeyBindings.h>
#include <LInput/Buttons/ButtonStates.h>
#include <LInput/Mouse/MouseButton.h>
#include <LInput/Win32/RawInput/RawInput.h>
#include <LInput/Buttons/Extensions/ButtonsStdExtension.h>

#include "OIVImage/OIVBaseImage.h"
#include "LabelManager.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"
#include "MouseCaptureState.h"
#include "ContextMenu.h"
#include "win32/FileWatcherWin32.h"
#include "ViewerRenderPort.h"
#include <OIVAppCore/AppSettingsPolicy.h>
#include <OIVAppCore/FileList.h>
#include <OIVAppCore/FileReloadPolicy.h>
#include <OIVAppCore/FileSessionController.h>
#include <OIVAppCore/FileRemovalPolicy.h>
#include <OIVAppCore/IFileListProvider.h>
#include <OIVAppCore/ImageLoadController.h>
#include <OIVAppCore/ImageState.h>
#include <OIVAppCore/OIVImageHelper.h>
#include <OIVAppCore/SelectionRect.h>
#include <OIVAppCore/SlideshowPolicy.h>
#include <OIVAppCore/ViewerPresentationPolicy.h>
#include <OIVShared/ViewTransformController.h>

#include "MouseMultiClickHandler.h"
#include "UI/MessageManager.h"

#include <NetSettings/GuiProvider.h>
#include <ImageLoader.h>
#include <ImageCodec.h>
#include <vector>
#include "win32/EventSync.h"
#include "InterThreadMessages.h"
#include <OIVShared/ImageResidency.h>
namespace OIV
{
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
        ResetAll = 1,
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
                    // MessageBox(nullptr, L"12", L"12", MB_OK);
                    fLastTap = high_resolution_clock::time_point::min();
                }
                fLastTap = high_resolution_clock::now();
            }
        }
    };

    class ViewerApplication : public IFileListProvider
    {
      public:

        void OnLabelRefreshRequest();
        ViewerApplication();
        ~ViewerApplication();
        void Init(std::wstring filePath);
        void Run();
        static std::wstring GetAppDataFolder();
        static HWND FindTrayBarWindow();

      private:  // types

        struct CommandRequestIntenal
        {
            std::string commandName;
            std::string args;
        };

      private:  // methods

        std::wstring GetLogFilePath();
        void HandleException(bool isFromLibrary, LLUtils::Exception::EventArgs args, std::wstring seperatedCallStack);
#pragma region Win32 event handling
        bool handleKeyInput(const ::Win32::EventWinMessage* evnt);
        LRESULT ClientWindwMessage(const ::Win32::Event* evnt1);
        void SetTopMostUserMesage();
        void ProcessTopMost();
        void SetAppActive(bool active);
        bool GetAppActive() const;

        bool HandleWinMessageEvent(const ::Win32::EventWinMessage* evnt);
        void CloseApplication(bool closeToTray);
        bool HandleFileDragDropEvent(const ::Win32::EventDdragDropFile* event_ddrag_drop_file);
        bool HandleMessages(const ::Win32::Event* evnt);
        bool HandleClientWindowMessages(const ::Win32::Event* evnt);
        double GetMinimumPixelSize();

#pragma endregion Win32 event handling
        void AddCommandsAndKeyBindings();
        void OnMonitorChanged(const EventManager::MonitorChangeEventParams& params);
        void ProbeForMonitorChange();
        void PerformRefresh();
        void SetUserMessage(const std::wstring& message, GroupID groupID = 0,
                            MessageFlags groupFlags = MessageFlags::Interchangeable);
        bool ExecuteCommandInternal(const CommandRequestIntenal& request);
        bool ExecuteCommand(const CommandManager::CommandRequest& request);
        bool ExecutePredefinedCommand(std::string command);
        void PostInitOperations();
#pragma region Commands
        void CMD_Zoom(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ViewState(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ToggleKeyBindings(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
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
        HWND GetWindowHandle() const;
        void UpdateTitle();
        // bool JumpTo(FileIndexType fileIndex);
        bool JumpFiles(FileList::index_type step);
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
        bool LoadFile(std::wstring filePath, IMCodec::PluginTraverseMode loaderFlags);
        bool LoadFileOrFolder(const std::wstring& filePath, IMCodec::PluginTraverseMode traverseMode);
        bool ProcessImageLoadResult(const ImageLoadResult& loadResult);
        void LoadOivImage(OIVBaseImageSharedPtr oivImage);
        void UpdateOpenImageUI();
        void UnloadWelcomeMessage();
        void ShowWelcomeMessage();
        const std::wstring& GetOpenedFileName() const;
        bool IsImageOpen() const;
        bool IsOpenedImageIsAFile() const;
        void LoadFileInFolder(std::wstring filePath);
        void TransformImage(IMUtil::AxisAlignedRotation transform, IMUtil::AxisAlignedFlip flip);
        void LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height, uint32_t rowPitch,
                     IMCodec::TexelFormat texelFormat);
        ClipboardDataType PasteFromClipBoard();
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
        bool IsMainThread() const { return fMainThreadID == GetCurrentThreadId(); }
        void OnFileChangedImpl(const IFileWatcher::FileChangedEventArgs*
                                   fileChangedEventArgs);  // file change handler, runs in the main thread.
        void OnFileChanged(IFileWatcher::FileChangedEventArgs fileChangedEventArgs);  // callback from file watcher
        void OnFileIndexResidencyReady(const std::wstring& fileName, IMCodec::ImageSharedPtr image);
        void OnFolderLoadResidencyReady(const BrowseResidencyManager::FileListSnapshot& snapshot,
                                        const std::wstring& fileName,
                                        IMCodec::ImageSharedPtr image);

        void ProcessCurrentFileChanged();
        void ProcessRemovalOfOpenedFile(const std::wstring& fileName);

        void OnNotificationIcon(::Win32::NotificationIconGroup::NotificationIconEventArgs args);
        void DelayResamplingCallback();
        void ShowImageInfo();
        void CountColorsAsync();
        void SetImageInfoVisible(bool visible);
        bool GetImageInfoVisible() const;
        void ProcessLoadedDirectory();
        void PerformReloadFile(const std::wstring& requestedFile);
        void HandleReloadAction(ReloadAction action, const std::wstring& requestedFile);
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

        using netsettings_Create_func = void (*)(GuiCreateParams*);
        using netsettings_SetVisible_func = void (*)(bool);
        using netsettings_SaveSettings_func = void (*)();

        struct SettingsContext
        {
            bool created;
            netsettings_Create_func Create;
            netsettings_SetVisible_func SetVisible;
            netsettings_SaveSettings_func SaveSettings;
        } settingsContext{};

      private:  // member fields

#pragma region FrameLimiter
        const bool EnableFrameLimiter = true;
        std::chrono::high_resolution_clock::time_point fLastRefreshTime;
        ::Win32::HighPrecisionTimer fRefreshTimer;
        uint32_t fRefreshRateTimes1000 = 60'000;
        ::Win32::MonitorDesc fCurrentMonitorProperties{};
        MonitorProvider fMonitorProvider;
#pragma endregion FrameLimiter
        Win32::MainWindow fWindow;
        AutoScrollUniquePtr fAutoScroll;
        RecursiveDelayedOp fRefreshOperation;
        RecursiveDelayedOp fPreserveImageSpaceSelection;
        double fMaxPixelSize = 30.0;
        double fMinImageSize = 150.0;
        SlideshowPolicy fSlideshowPolicy;
        bool fReloadSettingsFileIfChanged = false;
        IFileWatcher::FolderID fCOnfigurationFolderID = 0;
        int fCurrentFrame = 0;
        double fCurrentSequencerSpeed = 1.0;
        OIVBaseImageSharedPtr fCountingImageColor;
        std::atomic_bool fIsColorThreadRunning = false;
        std::thread fCountingColorsThread;
        // FileCache fFileCache;

        using MouseButtonType = LInput::MouseButton;
        template <typename T>
        using DeviceGroup = std::map<uint8_t, T>;

        using MouseButtonstate = LInput::ButtonsState<MouseButtonType, 8>;
        using MouseGroup = DeviceGroup<MouseButtonstate>;

        MouseGroup fMouseDevicesState;
        LInput::RawInput fRawInput;
        void OnRawInput(const LInput::RawInput::RawInputEvent& evnt);
        void OnMouseEvent(const LInput::ButtonStdExtension<MouseButtonType>::ButtonEvent& btnEvent);
        void OnMouseInput(const LInput::RawInput::RawInputEventMouse& mouseInput);

        MouseCaptureState fMouseCaptureState;

        bool fIsGridEnabled = false;
        OIV_PROP_TransparencyMode fTransparencyMode = OIV_PROP_TransparencyMode::TM_Medium;
        OIVBaseImageSharedPtr fAutoScrollAnchor;
        DWORD fMainThreadID = GetCurrentThreadId();
        SelectionRect fSelectionRect;
        uint32_t fQueueResamplingDelay = 50;
        LLUtils::RectI32 fImageSpaceSelection = LLUtils::RectI32::Zero;
        ::Win32::Timer fTimerTopMostRetention;
        ::Win32::Timer fTimerSlideShow;
        ::Win32::Clipboard fClipboardHelper;

        int fTopMostCounter = 0;
        ::Win32::Timer fTimerNoActiveZoom;
        ::Win32::Timer fTimerNavigation;
        bool fIsResamplingEnabled = false;
        bool fQueueImageInfoLoad = false;
        uint16_t fQuickBrowseDelay = 100;
        bool fDisplayBiggestSubImageOnLoad = true;

        /// determines whether the current loaded file is the initial file being loaded at startup
        bool fIsTryToLoadInitialFile = false;
        bool fIsFirstFrameDisplayed = false;
        bool fIsOffsetLocked = false;
        bool fIsLockFitToScreen = false;
        bool fShowBorders = true;
        bool fImageInfoVisible = false;
        bool fIsActive = false;
        bool fRockerGestureActivate = false;
        LLUtils::PointF64 fDPIadjustmentFactor{1.0, 1.0};
        IMCodec::ImageLoader fImageLoader;
        std::unique_ptr<ImageLoadController> fImageLoadController;
        //::Win32::ClipboardFormatType fRTFFormatID {};
        //::Win32::ClipboardFormatType fHTMLFormatID {};

        DeletedFileRemovalMode fDeletedFileRemovalMode = DeletedFileRemovalMode::DeletedInternally;

        std::wstring fRequestedFileForRemoval;
        FileReloadPolicy fFileReloadPolicy;
        LLUtils::PointF64 fImageMargins{0.75, 0.75};
        std::wstring DefaultTextKeyColorTag = L"<textcolor=#ff8930ff>";
        std::wstring DefaultTextValueColorTag = L"<textcolor=#7672ffff>";
        LLUtils::StopWatch fFileDisplayTimer;
        MouseMultiClickHandler fMouseClickEventHandler{500, 2};
        void OnMouseMultiClick(const MouseMultiClickHandler::EventArgs& args);

        ResetTransformationMode fResetTransformationMode = ResetTransformationMode::ResetAll;
        const OIV_CMD_ColorExposure_Request DefaultColorCorrection = {1.0, 0.0, 1.0, 1.0, 1.0};
        OIV_CMD_ColorExposure_Request fColorExposure = DefaultColorCorrection;
        OIV_CMD_ColorExposure_Request fLastColorExposure = fColorExposure;
        VirtualStatusBar fVirtualStatusBar;

        AdaptiveMotion fAdaptiveZoom = AdaptiveMotion(1.0, 0.6, 1.0);
        AdaptiveMotion fAdaptivePanLeftRight = AdaptiveMotion(1.6, 1.0, 5.2);
        AdaptiveMotion fAdaptivePanUpDown = AdaptiveMotion(1.6, 1.0, 5.2);
        ImageState fImageState;

        CommandController fCommandController;
        OivRenderGateway fRenderGateway;
        std::unique_ptr<FreeType::FreeTypeConnector> fFreeType;
        LabelManager fLabelManager;
        KeyDoubleTap fDoubleTap;
        DownscalingTechnique fDownScalingTechnique = DownscalingTechnique::Software;
        Win32::FileWatcherWin32 fFileWatcher;
        std::wstring fCurrentFolderWatched;
        std::set<std::wstring> fKnownFileTypesSet;
        std::wstring fKnownFileTypes;
        ::Win32::FileDialogFilterBuilder fOpenComDlgFilters;
        ::Win32::FileDialogFilterBuilder fSaveComDlgFilters;
        std::wstring fDefaultSaveFileExtension = L"png";
        int16_t fDefaultSaveFileFormatIndex = -1;
        std::wstring fPendingFolderLoad;
        LLUtils::StopWatch fLastImageLoadTimeStamp;
        ::Win32::NotificationIconGroup fNotificationIcons;
        ::Win32::NotificationIconGroup::IconID fNotificationIconID;
        std::unique_ptr<MessageManager> fMessageManager;
        void OnSettingChange(const std::wstring& key, const std::wstring& value);
        void LoadSettings();
        void SetResamplingEnabled(bool enable);
        bool GetResamplingEnabled() const;
        void QueueResampling();
        void SortFileList();

        std::unique_ptr<ContextMenu<int>> fNotificationContextMenu;
        std::shared_ptr<OIVFileImage> fInitialFile;

        LLUtils::LogFile mLogFile{GetLogFilePath(), true};

        struct BindingElement
        {
            std::string commandDescription;
            std::string command;
            std::string arguments;
        };
        LInput::KeyBindings<BindingElement> fKeyBindings;

        struct MenuItemData
        {
            std::string command;
            std::string args;
        };

        std::unique_ptr<ContextMenu<MenuItemData>> fContextMenu;
        LLUtils::PointI32 fDownPosition;
        ::Win32::Timer fContextMenuTimer;
        ::Win32::Timer fSequencerTimer;
        FileSorter fFileSorter;
        EventSync fEventSync;
        std::atomic_bool fIsShuttingDown = false;
        ImageResidency fImageResidency;
        std::unique_ptr<FileSessionController> fFileSessionController;

        // FileListProvider
        virtual FileListStringType GetActiveFileName() override;
        
    };
}  // namespace OIV
