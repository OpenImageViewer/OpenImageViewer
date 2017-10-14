#pragma once

#include <mutex>

#include "win32/Win32Window.h"
#include "API/defs.h"
#include <Utility.h>
#include "AutoScroll.h"
#include "ImageDescriptor.h"
#include "UserSettings.h"
#include <Rect.h>
#include "RecursiveDelayOp.h"
#include "AdaptiveMotion.h"

namespace OIV
{
    enum ImageSizeType
    {
          IST_Original
        , IST_Transformed
        , IST_Visible
    };


    class TestApp
    {
    public:
        TestApp();
        
        ~TestApp();
        void Init(std::wstring filePath);
        void Run();
        void Destroy();
   
    private: //methods
#pragma region Win32 event handling
        void handleKeyInput(const Win32::EventWinMessage* evnt);
        bool HandleWinMessageEvent(const Win32::EventWinMessage* evnt);
        bool HandleFileDragDropEvent(const Win32::EventDdragDropFile* event_ddrag_drop_file);
        void HandleRawInputMouse(const Win32::EventRawInputMouseStateChanged* evnt);
        bool HandleMessages(const Win32::Event* evnt);
#pragma endregion Win32 event handling
        
        void PostInitOperations();
        template<class T, class U>
        ResultCode ExecuteCommand(CommandExecute command, T * request, U * response);
        void OnRefresh();
        HWND GetWindowHandle() const;
        void UpdateTitle();
        void UpdateStatusBar();
        void UpdateUIFileIndex();
        bool JumpFiles(int step);
        void ToggleFullScreen();
        void ToggleBorders();
        void ToggleSlideShow();
        void SetFilterLevel(OIV_Filter_type filterType);
        void ToggleGrid();
        void Pan(const LLUtils::PointF64& panAmount);
        void Zoom(double precentage, int zoomX = -1, int zoomY = -1);
        void FitToClientAreaAndCenter();
        LLUtils::PointF64 GetImageSize(ImageSizeType type);
        void UpdateUIZoom();
        void SetZoom(double zoom, int x = -1, int y = -1);
        void UpdateCanvasSize();
        LLUtils::PointF64 ClientToImage(LLUtils::PointI32 clientPos) const;
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
        void FinalizeImageLoad(ResultCode result);
        void FinalizeImageLoadThreadSafe(ResultCode result);
        bool LoadFileFromBuffer(const uint8_t* buffer, const std::size_t size, std::string extension, bool onlyRegisteredExtension);
        void ReloadFileInFolder();
        void UpdateOpenedFileIndex();   
        void LoadFileInFolder(std::wstring filePath);
        void TransformImage(OIV_AxisAlignedRTransform transform);
        void LoadRaw(const uint8_t* buffer, uint32_t width, uint32_t height,uint32_t rowPitch, OIV_TexelFormat texelFormat);
        void PasteFromClipBoard();
        void CopyVisibleToClipBoard();
        void CropVisibleImage();
        void DisplayImage(ImageDescriptor& descriptor, bool resetScrollState) ;
        void UnloadOpenedImaged();
        void DeleteOpenedFile(bool permanently);
        void UpdateExposure();
        void ToggleColorCorrection(); 
        

    private: // member fields
        Win32::Win32WIndow fWindow;
        AutoScroll fAutoScroll = AutoScroll(&fWindow, std::bind(&TestApp::OnScroll, this, std::placeholders::_1));
        OIV_Filter_type fFilterType = OIV_Filter_type::FT_Linear;
        RecrusiveDelayedOp fRefreshOperation;
        bool fIsSlideShowActive = false;
        int fKeyboardPanSpeed = 1;
        double fKeyboardZoomSpeed = 0.1;
        double fIsGridEnabled = false;
        double fZoom = 1.0;
        ImageDescriptor fImageBeingOpened;
        ImageDescriptor fOpenedImage;
        DWORD fMainThreadID = GetCurrentThreadId();
        std::mutex fMutexWindowCreation;
        LLUtils::RectI32 fSelectionRect = { {-1,-1},{-1,-1} };
        int cTimerID = 1500;
        LLUtils::ListWString::size_type fCurrentFileIndex = std::numeric_limits<LLUtils::ListWString::size_type>::max();
        LLUtils::ListWString fListFiles;
        LLUtils::PointI32 fDragStart = { -1,-1 };
        UserSettings fSettings;
        bool fIsInitialLoad = false;
        bool fUseRainbowNormalization = false;
        LLUtils::PointF64 fOffset = LLUtils::PointF64::Zero;
        bool fIsOffsetLocked = false;
        bool fIsLockFitToScreen = false;
        OIV_CMD_ColorExposure_Request fColorExposure = { 1.0, 0.0, 1.0 };
        OIV_CMD_ColorExposure_Request fLastColorExposure = fColorExposure;
        OIV_CMD_QueryImageInfo_Response fVisibleFileInfo = {};
        AdaptiveMotion fAdaptiveZoom = AdaptiveMotion(1.0, 0.6, 1.0);
        AdaptiveMotion fAdaptivePanLeftRight = AdaptiveMotion(1.6, 1.0, 5.2);
        AdaptiveMotion fAdaptivePanUpDown = AdaptiveMotion(1.6, 1.0, 5.2);
    };
}
