#pragma once
#include "win32/Win32Window.h"

namespace OIV
{
    class TestApp
    {
    public:
        TestApp();
        ~TestApp();
        HWND GetWindowHandle();
        void Run(std::wstring filePath);
        void UpdateFileInddex();
        void JumpFiles(int step);

        void ToggleFullScreen();
        void ToggleBorders();
        void ToggleSlideShow();
        void SetFilterLevel(int filterLevel);
        void ToggleGrid();
        void handleKeyInput(const Win32WIndow::Win32Event& evnt);
        void Pan(int horizontalPIxels, int verticalPixels);
        void Zoom(double precentage, int zoomX = -1 , int zoomY = -1);
        void UpdateCanvasSize();
        void UpdateTexelPos();
        bool HandleMessages(const Win32WIndow::Win32Event& evnt);
        template<class T, class U>
        bool ExecuteCommand(CommandExecute command, T * request, U * response);

    private:
        Win32WIndow fWindow;
        int fFilterlevel;
        bool fIsSlideShowActive;
        int fKeyboardPanSpeed;
        double fKeyboardZoomSpeed;
        double fIsGridEnabled;

        static const int cTimerID = 1500;
        ListFilesIterator fCurrentListPosition;
        ListFiles fListFiles;
        void UpdateFileInfo(const CmdResponseLoad& load_response);
        bool LoadFile(std::wstring filePath, bool onlyRegisteredExtension = true);
        void LoadFileInFolder(std::wstring filePath);
    };
}
