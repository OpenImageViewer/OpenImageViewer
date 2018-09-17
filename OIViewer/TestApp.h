#pragma once

#include <mutex>

#include "win32/Win32Window.h"
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

namespace OIV
{
    enum class ImageSizeType
    {
          Original
        , Transformed
        , Visible
    };


    class TestApp
    {
    public:
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
        void UpdateRefreshRate();
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
        
#pragma endregion //Commands
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
        void UpdateImageProperties();
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
        void UpdateWindowSize();
        void Center();
        LLUtils::PointF64 ResolveOffset(const LLUtils::PointF64& point);
        void UpdateVisibleImageInfo();
        void SetOffset(LLUtils::PointF64 offset);
        void SetOriginalSize();
        void OnScroll(const LLUtils::PointF64& panAmount);
        bool LoadFile(std::wstring filePath, bool onlyRegisteredExtension);
        void SetOpenImage(const ImageDescriptor& image_descriptor);
        void UnloadWelcomeMessage();
        void ShowWelcomeMessage();
        void FinalizeImageLoad(ResultCode result);
        void FinalizeImageLoadThreadSafe(ResultCode result);
        bool LoadFileFromBuffer(const uint8_t* buffer, const std::size_t size, std::string extension, bool onlyRegisteredExtension);
        void ReloadFileInFolder();
        void UpdateOpenedFileIndex();   
        void LoadFileInFolder(std::wstring filePath);
        void TransformImage(OIV_AxisAlignedRTransform transform);
        void LoadRaw(const std::byte* buffer, uint32_t width, uint32_t height,uint32_t rowPitch, OIV_TexelFormat texelFormat);
        void PasteFromClipBoard();
        void CopyVisibleToClipBoard();
        void CropVisibleImage();
        void DisplayImage(ImageDescriptor& descriptor, bool resetScrollState) ;
        void UnloadOpenedImaged();
        void DeleteOpenedFile(bool permanently);
        void UpdateExposure();
        bool ToggleColorCorrection(); 
        

    private: // member fields
#pragma region FrameLimiter
        const bool EnableFrameLimiter = true;
        std::chrono::high_resolution_clock::time_point fLastRefreshTime;
        Win32::HighPrecisionTimer fRefreshTimer;
        HMONITOR fLastMonitor = nullptr;
        bool fAppFullyInitialized = false;
        uint32_t fRefreshRateTimes1000 = 60'000;
#pragma endregion FrameLimiter
        Win32::Win32WIndow fWindow;
        AutoScroll fAutoScroll = AutoScroll(&fWindow, std::bind(&TestApp::OnScroll, this, std::placeholders::_1));
        RecrusiveDelayedOp fRefreshOperation;
        RecrusiveDelayedOp fPreserveImageSpaceSelection;
        bool fIsSlideShowActive = false;
        int fKeyboardPanSpeed = 1;
        double fKeyboardZoomSpeed = 0.1;
        bool fIsGridEnabled = false;
        ImageDescriptor fImageBeingOpened;
        ImageDescriptor fOpenedImage;
        DWORD fMainThreadID = GetCurrentThreadId();
        std::mutex fMutexWindowCreation;
        SelectionRect fSelectionRect;
        const int cTimerID = 1500;
        const int cTimerIDHideUserMessage = 1000;
        uint32_t fMinDelayRemoveMessage = 1000;
        uint32_t fDelayPerCharacter = 40;
        LLUtils::RectI32 fImageSpaceSelection = LLUtils::RectI32::Zero;

        inline static const OIVString sFontPath = OIV_ToOIVString(L"C:\\Windows\\Fonts\\consola.ttf");
        inline static const OIVCHAR* sFontPathCstr = sFontPath.c_str();

        static constexpr int FileIndexEnd = std::numeric_limits<int>::max();
        static constexpr int FileIndexStart = std::numeric_limits<int>::min();
        int fCurrentFileIndex = FileIndexStart;
        LLUtils::ListWString fListFiles;
        LLUtils::PointI32 fDragStart = { -1,-1 };
        UserSettings fSettings;
        bool fIsInitialLoad = false;
        bool fUseRainbowNormalization = false;
        bool fIsOffsetLocked = false;
        bool fIsLockFitToScreen = false;
        const OIV_CMD_ColorExposure_Request DefaultColorCorrection = { 1.0,0.0,1.0,1.0,1.0 };
        OIV_CMD_ColorExposure_Request fColorExposure = DefaultColorCorrection;
        OIV_CMD_ColorExposure_Request fLastColorExposure = fColorExposure;
        
        OIV_CMD_QueryImageInfo_Response fVisibleFileInfo = {};
        AdaptiveMotion fAdaptiveZoom = AdaptiveMotion(1.0, 0.6, 1.0);
        AdaptiveMotion fAdaptivePanLeftRight = AdaptiveMotion(1.6, 1.0, 5.2);
        AdaptiveMotion fAdaptivePanUpDown = AdaptiveMotion(1.6, 1.0, 5.2);
        OIV_CMD_ImageProperties_Request fImageProperties;
        OIV_CMD_ImageProperties_Request fUserMessageOverlayProperties;
        OIV_CMD_ImageProperties_Request fDebugMessageOverlayProperties;
        CommandManager fCommandManager;
        ImageHandle fKeybindingsHandle = ImageHandleNull;
        ImageHandle fWelcomeMessageHandle = ImageHandleNull;

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
