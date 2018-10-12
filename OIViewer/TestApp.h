#pragma once

#include <mutex>

#include "win32/MainWindow.h"
#include "win32/HighPrecisionTimer.h"
#include "API/defs.h"
#include "API/StringHelper.h"
#include <Utility.h>
#include "AutoScroll.h"
#include "ImageDescriptor.h"
#include "UserSettings.h"
#include <Rect.h>
#include "RecursiveDelayOp.h"
#include "AdaptiveMotion.h"
#include "CommandManager.h"
#include "Keyboard/KeyBindings.h"
#include "SelectionRect.h"
#include "OIVImage\OIVBaseImage.h"
#include "LabelManager.h"
#include "win32/MonitorInfo.h"
#include "VirtualStatusBar.h"
#include "MonitorProvider.h"


namespace OIV
{
    struct MonitorDesc;
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

    enum class ImageChainStage
    {
          Opened
        , Begin = Opened
        , Deformed
        , Rasterized
        , Resampled
        , Count
    };

    struct ImageChain
    {
        std::array<OIVBaseImageSharedPtr, static_cast<size_t>(ImageChainStage::Count)> Chain;
        OIVBaseImageSharedPtr& Get(ImageChainStage stage)
        {
            return Chain[static_cast<size_t>(stage)];
        }

        const OIVBaseImageSharedPtr& Get(ImageChainStage stage) const
        {
            return const_cast<ImageChain*>(this)->Get(stage);
            //return Chain[static_cast<size_t>(stage)];
        }

        void Reset()
        {
            for (auto& e : Chain)
                e.reset();
        }
    };


    

    class TestApp
    {
    public:
        void OnLabelRefreshRequest();
        TestApp();
        void Init(std::wstring filePath);
        void Run();
        void Destroy();

    private: //methods
#pragma region Win32 event handling
        bool handleKeyInput(const Win32::EventWinMessage* evnt);
        void HideUserMessage();
        bool HandleWinMessageEvent(const Win32::EventWinMessage* evnt);
        bool HandleFileDragDropEvent(const Win32::EventDdragDropFile* event_ddrag_drop_file);
        void HandleRawInputMouse(const Win32::EventRawInputMouseStateChanged* evnt);
        bool HandleMessages(const Win32::Event* evnt);
#pragma endregion Win32 event handling
        void AddCommandsAndKeyBindings();
        void OnMonitorChanged(const EventManager::MonitorChangeEventParams& params);
        void ProbeForMonitorChange();
        void PerformRefresh();
        void SetUserMessage(const std::wstring& message);
        void SetDebugMessage(const std::string& message);
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
        bool JumpFiles(int step);
        void ToggleFullScreen();
        void ToggleBorders();
        void ToggleSlideShow();
        void SetFilterLevel(OIV_Filter_type filterType);
        OIV_Filter_type GetFilterType() const;
        void ToggleGrid();
        void Pan(const LLUtils::PointF64& panAmount);
        void Zoom(double precentage, int zoomX = -1, int zoomY = -1);
        void ZoomInternal(double amount, int zoomX, int zoomY);
        void FitToClientAreaAndCenter();
        LLUtils::PointF64 GetImageSize(ImageSizeType type);
        void UpdateUIZoom();
        void SaveImageSpaceSelection();
        void LoadImageSpaceSelection();
        void SetZoomInternal(double zoom, int x = -1, int y = -1);
        double GetScale() const;
        LLUtils::PointF64 GetOffset() const;
        LLUtils::PointF64 ImageToClient(LLUtils::PointF64 imagepos) const;
        LLUtils::RectF64 ImageToClient(LLUtils::RectF64 clientRect) const;
        void UpdateCanvasSize();
        LLUtils::PointF64 ClientToImage(LLUtils::PointI32 clientPos) const;
        LLUtils::RectF64 ClientToImage(LLUtils::RectI32 clientRect) const;
        void UpdateTexelPos();
        void AutoPlaceImage();
        void UpdateWindowSize();
        void Center();
        LLUtils::PointF64 ResolveOffset(const LLUtils::PointF64& point);
        void SetOffset(LLUtils::PointF64 offset);
        void SetOriginalSize();
        void OnScroll(const LLUtils::PointF64& panAmount);
        bool LoadFile(std::wstring filePath, bool onlyRegisteredExtension);
        void UpdateOpenImageUI();
        void SetOpenImage(const OIVBaseImageSharedPtr& image_descriptor);
        void UnloadWelcomeMessage();
        void ShowWelcomeMessage();
        void FinalizeImageLoad(ResultCode result);
        void FinalizeImageLoadThreadSafe(ResultCode result);
        const std::wstring& GetOpenedFileName() const;
        bool IsOpenedImageIsAFile() const;
        void ReloadFileInFolder();
        void UpdateOpenedFileIndex();   
        void LoadFileInFolder(std::wstring filePath);
        void TransformImage(OIV_AxisAlignedRotation transform, OIV_AxisAlignedFlip flip);
        void LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height,uint32_t rowPitch, OIV_TexelFormat texelFormat);
        void PasteFromClipBoard();
        void CopyVisibleToClipBoard();
        void CropVisibleImage();
        void AfterFirstFrameDisplayed();
        void DisplayImage(OIVBaseImageSharedPtr& descriptor);
        void UnloadOpenedImaged();
        void DeleteOpenedFile(bool permanently);
        void RefreshImage();
        void DisplayOpenedFileName();
        void UpdateExposure();
        bool ToggleColorCorrection(); 
        void CancelSelection();
        

    private: // member fields
#pragma region FrameLimiter
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
        bool fIsSlideShowActive = false;
        int fKeyboardPanSpeed = 1;
        double fKeyboardZoomSpeed = 0.1;
        bool fIsGridEnabled = false;
        OIVBaseImageSharedPtr fAutoScrollAnchor;
        DWORD fMainThreadID = GetCurrentThreadId();
        std::mutex fMutexWindowCreation;
        SelectionRect fSelectionRect;
        const int cTimerID = 1500;
        const int cTimerIDHideUserMessage = 1000;
        uint32_t fMinDelayRemoveMessage = 1000;
        uint32_t fDelayPerCharacter = 40;
        LLUtils::RectI32 fImageSpaceSelection = LLUtils::RectI32::Zero;
        OIV_AxisAlignedRotation fAxisAlignedTransform = OIV_AxisAlignedRotation::AAT_None;
        OIV_AxisAlignedFlip     fAxisAlignedFlip = OIV_AxisAlignedFlip::AAF_None;

        static constexpr int FileIndexEnd = std::numeric_limits<int>::max();
        static constexpr int FileIndexStart = std::numeric_limits<int>::min();
        int fCurrentFileIndex = FileIndexStart;
        LLUtils::ListWString fListFiles;
        LLUtils::PointI32 fDragStart = { -1,-1 };
        UserSettings fSettings;
        bool fIsInitialLoad = false;
        bool fIsFirstFrameDisplayed = false;
        bool fUseRainbowNormalization = false;
        bool fIsOffsetLocked = false;
        bool fIsLockFitToScreen = false;
        bool fShowBorders = true;
        ResetTransformationMode fResetTransformationMode = ResetTransformationMode::ResetAll;
        const OIV_CMD_ColorExposure_Request DefaultColorCorrection = { 1.0,0.0,1.0,1.0,1.0 };
        OIV_CMD_ColorExposure_Request fColorExposure = DefaultColorCorrection;
        OIV_CMD_ColorExposure_Request fLastColorExposure = fColorExposure;
        VirtualStatusBar fVirtualStatusBar;

        AdaptiveMotion fAdaptiveZoom = AdaptiveMotion(1.0, 0.6, 1.0);
        AdaptiveMotion fAdaptivePanLeftRight = AdaptiveMotion(1.6, 1.0, 5.2);
        AdaptiveMotion fAdaptivePanUpDown = AdaptiveMotion(1.6, 1.0, 5.2);
        ImageChain fCurrentImageChain;
        ImageChain fPreviousImageChain;


        CommandManager fCommandManager;
        LabelManager fLabelManager;
        

        
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
    };
}
