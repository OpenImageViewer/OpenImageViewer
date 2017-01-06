#pragma once
#include "win32/Win32Window.h"
#include "API/defs.h"
#include <Utility.h>
#include "StopWatch.h"
#include "AutoScroll.h"

namespace OIV
{
    class TestApp
    {
    public:
        TestApp();
        ~TestApp();
        HWND GetWindowHandle() const;
        void Run(std::wstring filePath);
        void UpdateFileInddex();
        void JumpFiles(int step);

        void ToggleFullScreen();
        void ToggleBorders();
        void ToggleSlideShow();
        void SetFilterLevel(int filterLevel);
        void ToggleGrid();
        
        void Pan(int horizontalPIxels, int verticalPixels);
        void Zoom(double precentage, int zoomX = -1 , int zoomY = -1);
        void UpdateCanvasSize();
        void UpdateTexelPos();
        void UpdateWindowSize(const Win32::EventWinMessage* winMessage);
#pragma region Win32 event handling
        void Rotate90Degree(bool clockwise);
        void handleKeyInput(const Win32::EventWinMessage* evnt);

        bool HandleWinMessageEvent(const Win32::EventWinMessage* evnt);
        bool HandleFileDragDropEvent(const Win32::EventDdragDropFile* event_ddrag_drop_file);
        void HandleRawInputMouse(const Win32::EventRawInputMouseStateChanged* evnt);
        bool HandleMessages(const Win32::Event* evnt);
#pragma endregion Win32 event handling
        template<class T, class U>
        bool ExecuteCommand(CommandExecute command, T * request, U * response);

    private: //methods
        void OnScroll(int32_t x, int32_t y);
        void UpdateFileInfo(const CmdResponseLoad& load_response, const long double& totalLoadTime);
        bool LoadFile(std::wstring filePath, bool onlyRegisteredExtension = true);
        bool LoadFile(const uint8_t* buffer, const std::size_t size, std::string extension, bool onlyRegisteredExtension);
        void LoadFileInFolder(std::wstring filePath);
        

    private:
        Win32::Win32WIndow fWindow;
        AutoScroll fAutoScroll = AutoScroll(&fWindow, std::bind(&TestApp::OnScroll, this, std::placeholders::_1, std::placeholders::_2));

        int fFilterlevel;
        bool fIsSlideShowActive;
        int fKeyboardPanSpeed;
        double fKeyboardZoomSpeed;
        double fIsGridEnabled;
        std::wstring fOpenedFile;
        int cTimerID = 1500;
        ListFiles::size_type fCurrentFileIndex;
        ListFiles fListFiles;
        
    };
}
