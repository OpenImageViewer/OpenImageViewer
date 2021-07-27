#pragma once

#include <mutex>

#include "win32/MainWindow.h"
#include "win32/HighPrecisionTimer.h"
#include "win32/Timer.h"
#include <defs.h>
#include <LLUtils/Utility.h>
#include "AutoScroll.h"
#include "ImageDescriptor.h"
#include "UserSettings.h"
#include <LLUtils/Rect.h>
#include "RecursiveDelayOp.h"
#include "AdaptiveMotion.h"
#include "CommandManager.h"
#include "Keyboard/KeyBindings.h"
#include "Keyboard/KeyDoubleTap.h"
#include "SelectionRect.h"
#include "OIVImage\OIVBaseImage.h"
#include "LabelManager.h"
#include "win32/MonitorInfo.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"
#include "Helpers/OIVImageHelper.h"
#include "ImageState.h"
#include <LLUtils/Logging/LogFile.h>
#include "ContextMenu.h"
#include "FileWatcher.h"
#include "win32/NotificationIconGroup.h"

namespace OIV
{
    enum class ImageSizeType
    {
          Original
        , Transformed
        , Visible
    };


    //Determines whether to change zoom / pan when loading a new file
    enum class ResetTransformationMode
    {
          DoNothing = 0
        , ResetAll = 1
        , Count
    };

    enum class DownscalingTechnique
    {
           None
         , HardwareMipmaps
         , Software
         , Count
    };

   

    //Assue Count exists and presenting the total number of values in an enum.
    template <typename T, typename UnderlyingType = typename std::underlying_type_t<T>>
    T GetNextEnumValue(T enumVal)
    {
     //   using UnderlyingType = std::underlying_type_t<T>;
        return static_cast<T>((static_cast<UnderlyingType>(enumVal) + static_cast<UnderlyingType>(1)) % static_cast<UnderlyingType>(T::Count));
    }

    

    class TestApp
    {
    public:
        void OnLabelRefreshRequest();
        TestApp();
        void Init(std::wstring filePath);
        void Run();
        void Destroy();
        static std::wstring GetAppDataFolder();
        static HWND FindTrayBarWindow();

    private:// types
        using FileIndexType = LLUtils::ListWString::difference_type;
        using FileCountType = LLUtils::ListWString::size_type;

    private: //methods

		std::wstring GetLogFilePath();
		void HandleException(bool isFromLibrary, LLUtils::Exception::EventArgs args);
#pragma region Win32 event handling
        bool handleKeyInput(const Win32::EventWinMessage* evnt);
        void HideUserMessageGradually();
        LRESULT ClientWindwMessage(const Win32::Event * evnt1);
        void SetTopMostUserMesage();
        void ProcessTopMost();
        bool HandleWinMessageEvent(const Win32::EventWinMessage* evnt);
		void CloseApplication(bool closeToTray);
        bool HandleFileDragDropEvent(const Win32::EventDdragDropFile* event_ddrag_drop_file);
        void HandleRawInputMouse(const Win32::EventRawInputMouseStateChanged* evnt);
        bool HandleMessages(const Win32::Event* evnt);
        bool HandleClientWindowMessages(const Win32::Event* evnt);
        
#pragma endregion Win32 event handling
        void AddCommandsAndKeyBindings();
        void OnMonitorChanged(const EventManager::MonitorChangeEventParams& params);
        void ProbeForMonitorChange();
        void PerformRefresh();
        void SetUserMessage(const std::wstring& message, int32_t hideDelay = 0);
        void SetUserMessageThreadSafe(const std::wstring& message, int32_t hideDelay = 0);
        void SetDebugMessage(const std::string& message);
        void HideUserMessage();
        bool ExecuteUserCommand(const CommandManager::CommandClientRequest&);
        void PostInitOperations();
#pragma region Commands
        void CMD_Zoom(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ViewState(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ToggleKeyBindings(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_OpenFile(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_AxisAlignedTransform(const CommandManager::CommandRequest&,CommandManager::CommandResult&);
        void CMD_ToggleColorCorrection(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        void CMD_ColorCorrection(const CommandManager::CommandRequest&, CommandManager::CommandResult&);
        double PerformColorOp(double& gamma, const std::string& cs, const std::string& val);
        void CMD_Pan(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_Placement(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_CopyToClipboard(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_PasteFromClipboard(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_ImageManipulation(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_Navigate(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_Shell(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        void CMD_DeleteFile(const CommandManager::CommandRequest& request, CommandManager::CommandResult& result);
        
#pragma endregion //Commands
        void OnSelectionRectChanged(const LLUtils::RectI32&, bool);
        void OnRefresh();
        void OnRefreshTimer();
        void OnPreserveSelectionRect();
        HWND GetWindowHandle() const;
        void UpdateTitle();
        void UpdateStatusBar();
        void UpdateUIFileIndex();
        bool JumpFiles(FileIndexType step);
		void ToggleFullScreen(bool multiFullScreen);
        void ToggleBorders();
        void ToggleSlideShow();
        void SetFilterLevel(OIV_Filter_type filterType);
        OIV_Filter_type GetFilterType() const;
        void ToggleGrid();
        void UpdateRenderViewParams();
        void Pan(const LLUtils::PointF64& panAmount);
        void Zoom(double precentage, int zoomX = -1, int zoomY = -1);
        void ZoomInternal(double amount, int zoomX, int zoomY);
        void FitToClientAreaAndCenter();
        LLUtils::PointF64 GetImageSize(ImageSizeType type);
        void UpdateUIZoom();
        void SaveImageSpaceSelection();
        void LoadImageSpaceSelection();
        void SetZoomInternal(double zoom, int x = -1, int y = -1, bool preserveFitToScreenState = false);
        double GetScale() const;
        LLUtils::PointF64 GetOffset() const;
        LLUtils::PointF64 ImageToClient(LLUtils::PointF64 imagepos) const;
        LLUtils::RectF64 ImageToClient(LLUtils::RectF64 clientRect) const;
        void UpdateCanvasSize();
        LLUtils::PointF64 ClientToImage(LLUtils::PointI32 clientPos) const;
        LLUtils::RectF64 ClientToImage(LLUtils::RectI32 clientRect) const;
        void UpdateTexelPos();
        void AutoPlaceImage(bool forceCenter = false);
        void UpdateWindowSize();
        void Center();
        LLUtils::PointF64 ResolveOffset(const LLUtils::PointF64& point);
        void SetOffset(LLUtils::PointF64 offset, bool preserveOffsetLockState = false);
        void SetOriginalSize();
        void OnScroll(const LLUtils::PointF64& panAmount);
        void OnImageSelectionChanged(const ImageList::ImageSelectionChangeArgs& ImageSelectionChangeArgs);
        bool LoadFile(std::wstring filePath, bool onlyRegisteredExtension);
        void UpdateOpenImageUI();
        void SetOpenImage(const OIVBaseImageSharedPtr& image_descriptor);
        void UnloadWelcomeMessage();
        void ShowWelcomeMessage();
        void FinalizeImageLoad(ResultCode result);
        void FinalizeImageLoadThreadSafe(ResultCode result);
        const std::wstring& GetOpenedFileName() const;
		bool IsImageOpen() const;
        bool IsOpenedImageIsAFile() const;
        void ReloadFileInFolder();
        void UpdateOpenedFileIndex();   
        void LoadFileInFolder(std::wstring filePath);
        void TransformImage(OIV_AxisAlignedRotation transform, OIV_AxisAlignedFlip flip);
        void LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height,uint32_t rowPitch, OIV_TexelFormat texelFormat);
        bool PasteFromClipBoard();
        void CopyVisibleToClipBoard();
        void CropVisibleImage();
        void AfterFirstFrameDisplayed();
        void UnloadOpenedImaged();
        void DeleteOpenedFile(bool permanently);
        void RefreshImage();
        void DisplayOpenedFileName();
        void UpdateExposure();
        bool ToggleColorCorrection(); 
        void CancelSelection();
        void LoadSubImages();
        void AddImageToControl(OIVBaseImageSharedPtr image, uint16_t imageSlot, uint16_t totalImages);
        void OnContextMenuTimer();
        void SetDownScalingTechnique(DownscalingTechnique technique);
        bool IsMainThread() const { return fMainThreadID == GetCurrentThreadId(); }
        void OnFileChanged(FileWatcher::FileChangedEventArgs fileChangedEventArgs);
        void ProcessCurrentFileChanged();
        void UpdateFileList(FileWatcher::FileChangedOp fileOp, const std::wstring& fileName);
        void WatchCurrentFolder();
        void OnNotificationIcon(Win32::NotificationIconGroup::NotificationIconEventArgs args);
		
    private: // member fields
#pragma region FrameLimiter
        static inline CmdNull NullCommand;
        const bool EnableFrameLimiter = true;
        std::chrono::high_resolution_clock::time_point fLastRefreshTime;
        Win32::HighPrecisionTimer fRefreshTimer;
        uint32_t fRefreshRateTimes1000 = 60'000;
        MonitorProvider fMonitorProvider;
#pragma endregion FrameLimiter
        Win32::MainWindow fWindow;
        AutoScrollUniquePtr fAutoScroll;
        RecrusiveDelayedOp fRefreshOperation;
        RecrusiveDelayedOp fPreserveImageSpaceSelection;
        int fKeyboardPanSpeed = 1;
        double fKeyboardZoomSpeed = 0.1;
        bool fIsGridEnabled = false;
        OIV_PROP_TransparencyMode fTransparencyMode = OIV_PROP_TransparencyMode::TM_Medium;
        OIVBaseImageSharedPtr fAutoScrollAnchor;
        DWORD fMainThreadID = GetCurrentThreadId();
        std::mutex fMutexWindowCreation;
        SelectionRect fSelectionRect;
        uint32_t fMinDelayRemoveMessage = 1000;
        uint32_t fDelayPerCharacter = 40;
        LLUtils::RectI32 fImageSpaceSelection = LLUtils::RectI32::Zero;
        Win32::Timer fTimerHideUserMessage;
        Win32::Timer fTimerTopMostRetention;
        Win32::Timer fTimerSlideShow;
        int fTopMostCounter = 0;
        Win32::Timer fTimerNoActiveZoom;
        Win32::Timer fTimerNavigation;
        

        static constexpr FileIndexType FileIndexEnd = std::numeric_limits<FileIndexType>::max();
        static constexpr FileIndexType FileIndexStart = std::numeric_limits<FileIndexType>::min();
        FileIndexType  fCurrentFileIndex = FileIndexStart;
        LLUtils::ListWString fListFiles;
        LLUtils::PointI32 fDragStart = { -1,-1 };
        UserSettings fSettings;
        bool fIsTryToLoadInitialFile = false; // determines whether the current loaded file is the initial file being loaded at startup
        bool fIsFirstFrameDisplayed = false;
        bool fIsOffsetLocked = false;
        bool fIsLockFitToScreen = false;
        bool fShowBorders = true;
        bool fFileReloadPending = false;
        LLUtils::Color DefaultTextKeyColor = 0xff8930ff;
        LLUtils::Color DefaultTextValueColor = 0x7672ffff;
        std::wstring DefaultTextKeyColorTag;
        std::wstring DefaultTextValueColorTag;

        ResetTransformationMode fResetTransformationMode = ResetTransformationMode::ResetAll;
        const OIV_CMD_ColorExposure_Request DefaultColorCorrection = { 1.0,0.0,1.0,1.0,1.0 };
        OIV_CMD_ColorExposure_Request fColorExposure = DefaultColorCorrection;
        OIV_CMD_ColorExposure_Request fLastColorExposure = fColorExposure;
        VirtualStatusBar fVirtualStatusBar;

        AdaptiveMotion fAdaptiveZoom = AdaptiveMotion(1.0, 0.6, 1.0);
        AdaptiveMotion fAdaptivePanLeftRight = AdaptiveMotion(1.6, 1.0, 5.2);
        AdaptiveMotion fAdaptivePanUpDown = AdaptiveMotion(1.6, 1.0, 5.2);
        ImageState fImageState;


        CommandManager fCommandManager;
        LabelManager fLabelManager;
        KeyDoubleTap fDoubleTap;
        DownscalingTechnique fDownScalingTechnique = DownscalingTechnique::Software;
        std::wstring fLastMessageForMainThread;
        FileWatcher fFileWatcher;
        std::wstring fCurrentFolderWatched;
        std::set<std::wstring> fKnownFileTypesSet;
        std::wstring fKnownFileTypes;
        LLUtils::StopWatch fLastImageLoadTimeStamp;
        Win32::NotificationIconGroup fNotificationIcons;
        Win32::NotificationIconGroup::IconID fNotificationIconID;

        std::unique_ptr<ContextMenu<int>> fNotificationContextMenu;

		LLUtils::LogFile mLogFile{ GetLogFilePath(), true };

        
        struct BindingElement
        {
            std::string commandDescription;
            std::string command;
            std::string arguments;
        };
        KeyBindings<BindingElement> fKeyBindings;

        struct CommandDesc
        {
            std::string description;
            std::string command;
            std::string arguments;
            std::string keybindings;
        };
        std::vector<CommandDesc> fCommandDescription;

        struct MenuItemData
        {
            std::string command;
            std::string args;
        };

        std::unique_ptr<ContextMenu<MenuItemData>> fContextMenu;
        LLUtils::PointI32 fDownPosition;
        
    	
        Win32::Timer fContextMenuTimer;

        struct 
        {
            bool operator() (const std::wstring& A, const std::wstring& B) const
            {
                using namespace LLUtils;
                using path = std::filesystem::path;
                path aPath(StringUtility::ToLower(A));
                std::wstring aName = aPath.stem();
                std::wstring aExt = aPath.extension();
                
                path bPath(StringUtility::ToLower(B));
                std::wstring bName = bPath.stem();
                std::wstring bExt = bPath.extension();

                return aName < bName || ((aName == bName) && aExt < bExt);
            }
        } const fFileListSorter;

    };
}
