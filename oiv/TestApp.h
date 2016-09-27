#pragma once
#include <windowsx.h>
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
        void HandleMessages(const MSG& uMsg);
        template<class T, class U>
        bool ExecuteCommand(CommandExecute command, T * request, U * response);

    private:
        int fFilterlevel;
        bool fIsSlideShowActive;
        static const int cTimerID = 1500;
        WINDOWPLACEMENT fLastWindowPlacement;
        ListFiles fListFiles;
        ListFilesIterator fCurrentListPosition;
        bool LoadFile(std::wstring filePath, bool onlyRegisteredExtension = true);
        void LoadFileInFolder(std::wstring filePath);
    };
}