#pragma once

#include <mutex>

#include "win32/Win32Window.h"
#include "API/defs.h"
#include <Utility.h>
#include "AutoScroll.h"
#include "ImageDescriptor.h"
#include "UserSettings.h"
#include <Rect.h>

namespace OIV
{
    class TestApp
    {
    public:
        TestApp();
        ~TestApp();
        void Init(std::wstring filePath);
        void Run();
        void Destroy();
        HWND GetWindowHandle() const;
        void UpdateTitle();
        void UpdateStatusBar();
        void UpdateZoomScrollState();
        void UpdateUIFileIndex();
        bool JumpFiles(int step);
        void ToggleFullScreen();
        void ToggleBorders();
        void ToggleSlideShow();
        void SetFilterLevel(OIV_Filter_type filterType);
        void ToggleGrid();
        
        void Pan(int horizontalPIxels, int verticalPixels);
        void Zoom(double precentage, int zoomX = -1 , int zoomY = -1);
        void UpdateCanvasSize();
        void UpdateTexelPos();
        void UpdateWindowSize();
#pragma region Win32 event handling
        void handleKeyInput(const Win32::EventWinMessage* evnt);

        bool HandleWinMessageEvent(const Win32::EventWinMessage* evnt);
        bool HandleFileDragDropEvent(const Win32::EventDdragDropFile* event_ddrag_drop_file);
        void HandleRawInputMouse(const Win32::EventRawInputMouseStateChanged* evnt);
        bool HandleMessages(const Win32::Event* evnt);
#pragma endregion Win32 event handling
        template<class T, class U>
        ResultCode ExecuteCommand(CommandExecute command, T * request, U * response);

    private: //methods
        void OnScroll(LLUtils::PointI32 panAmount);
        bool LoadFile(std::wstring filePath, bool onlyRegisteredExtension);
        void SetOpenImage(const ImageDescriptor& image_descriptor);
        void FinalizeImageLoad(ResultCode result);
        void FinalizeImageLoadThreadSafe(ResultCode result);
        bool LoadFileFromBuffer(const uint8_t* buffer, const std::size_t size, std::string extension, bool onlyRegisteredExtension);
        void ReloadFileInFolder();
        void UpdateOpenedFileIndex();   
        void LoadFileInFolder(std::wstring filePath);
        void TransformImage(OIV_AxisAlignedRTransform transform);
        void LoadRaw(const uint8_t* buffer, uint32_t width, uint32_t height, OIV_TexelFormat texelFormat);
        void PasteFromClipBoard();
        void CopyVisibleToClipBoard();
        void CropVisibleImage();
        void DisplayImage(ImageDescriptor& descriptor, bool resetScrollState) const;
        void UnloadOpenedImaged();
        void DeleteOpenedFile(bool permanently);
        

    private:
        Win32::Win32WIndow fWindow;
        AutoScroll fAutoScroll = AutoScroll(&fWindow, std::bind(&TestApp::OnScroll, this, std::placeholders::_1));
        OIV_Filter_type fFilterType = OIV_Filter_type::FT_Linear;
        
        bool fIsSlideShowActive = false;
        int fKeyboardPanSpeed = 100;
        double fKeyboardZoomSpeed = 0.1;
        double fIsGridEnabled = false;
        ImageDescriptor fImageBeingOpened;
        ImageDescriptor fOpenedImage;
        DWORD fMainThreadID = GetCurrentThreadId();
        std::mutex fMutexWindowCreation;
        LLUtils::RectI32 fSelectionRect = { {-1,-1},{-1,-1} };
        int cTimerID = 1500;
        LLUtils::ListString::size_type fCurrentFileIndex = std::numeric_limits<LLUtils::ListString::size_type>::max();
        LLUtils::ListString fListFiles;
        LLUtils::PointI32 fDragStart = { -1,-1 };
        UserSettings fSettings;
        bool fUpdateWindowOnInitialFileLoad = false;
        bool fUseRainbowNormalization = false;
    };
}
