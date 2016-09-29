#pragma once
#include <windowsx.h>
#include "win32/Win32Window.h"

namespace OIV
{
    class TestApp
    {
    public:
        TestApp();
        HWND GetWindowHandle();
        void Run(std::wstring filePath);

        void JumpFiles(int step);

        void ToggleFullScreen();
        void ToggleBorders();
        void ToggleSlideShow();
        void SetFilterLevel(int filterLevel);
        void handleKeyInput(const Win32WIndow::Win32Event& evnt);
        bool HandleMessages(const Win32WIndow::Win32Event& evnt);
        template<class T, class U>
        bool ExecuteCommand(CommandExecute command, T * request, U * response);

    private:
        Win32WIndow window;
        int fFilterlevel;
        bool fIsSlideShowActive;
        static const int cTimerID = 1500;

        ListFiles fListFiles;
        ListFilesIterator fCurrentListPosition;
        bool LoadFile(std::wstring filePath, bool onlyRegisteredExtension = true);
        void LoadFileInFolder(std::wstring filePath);
    };
}
