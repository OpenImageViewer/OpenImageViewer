#pragma once
#include "win32/Win32Window.h"
#include "API/defs.h"
#include <Utility.h>
#include "AutoScroll.h"
#include "FileLoadEntry.h"

namespace OIV
{
    class TestApp
    {
    public:
        TestApp();
        ~TestApp();
        HWND GetWindowHandle() const;
        void DisplayImage(ImageHandle image_handle);
        void UpdateTitle();
        void UpdateStatusBar();
        void Run(std::wstring filePath);
        void UpdateFileInddex();
        void JumpFiles(int step);

        void ToggleFullScreen();
        void ToggleBorders();
        void ToggleSlideShow();
        void SetFilterLevel(OIV_Filter_type filterType);
        void ToggleGrid();
        
        void Pan(int horizontalPIxels, int verticalPixels);
        void Zoom(double precentage, int zoomX = -1 , int zoomY = -1);
        void UpdateCanvasSize();
        void UpdateTexelPos();
        void UpdateWindowSize(const Win32::EventWinMessage* winMessage);
#pragma region Win32 event handling
        void TransformImage(OIV_AxisAlignedRTransform transform);
        void handleKeyInput(const Win32::EventWinMessage* evnt);

        bool HandleWinMessageEvent(const Win32::EventWinMessage* evnt);
        bool HandleFileDragDropEvent(const Win32::EventDdragDropFile* event_ddrag_drop_file);
        void HandleRawInputMouse(const Win32::EventRawInputMouseStateChanged* evnt);
        bool HandleMessages(const Win32::Event* evnt);
#pragma endregion Win32 event handling
        template<class T, class U>
        bool ExecuteCommand(CommandExecute command, T * request, U * response);

    private: //methods
        void OnScroll(LLUtils::PointI32 panAmount);
        
        void OnFileLoaded(std::wstring filePath);
        
        //void LoadFileAsync(std::wstring filePath, bool onlyRegisteredExtension);
        bool LoadFile(std::wstring filePath, bool onlyRegisteredExtension);
        void UnloadFile();
        bool FileLoaded(std::wstring filePath);
        void UpdateFileInfo(const OIV_CMD_LoadFile_Response& load_response, const long double& totalLoadTime);
        void FinalizeImageLoad();
        std::wstring BuildImageStringDesctriptor(const OIV_CMD_LoadFile_Response& loadresponse);
        void NotifyImageLoaded();
        bool LoadFileFromBuffer(const uint8_t* buffer, const std::size_t size, std::string extension, bool onlyRegisteredExtension);
        void LoadFileInFolder(std::wstring filePath);
        

    private:
        Win32::Win32WIndow fWindow;
        AutoScroll fAutoScroll = AutoScroll(&fWindow, std::bind(&TestApp::OnScroll, this, std::placeholders::_1));
        OIV_Filter_type fFilterType = OIV_Filter_type::FT_Linear;
        
        bool fIsSlideShowActive = false;
        int fKeyboardPanSpeed = 100;
        double fKeyboardZoomSpeed = 0.1;
        double fIsGridEnabled = false;
        FileLoadEntry fOpenedFile;
        DWORD fMainThreadID = GetCurrentThreadId();
        
        int cTimerID = 1500;
        LLUtils::ListString::size_type fCurrentFileIndex = std::numeric_limits<LLUtils::ListString::size_type>::max();
        LLUtils::ListString fListFiles;
        
    };
}
