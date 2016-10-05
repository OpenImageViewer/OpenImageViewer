#pragma once

#include <windows.h>
#include <Commctrl.h>
#include <vector>
#include <functional>
#include "DragAndDrop.h"


// Global variables
namespace OIV
{
    namespace Win32
    {
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
        class Win32WIndow
        {
        public:


        public:
            Win32WIndow();

            void ToggleFullScreen(bool multiMonitor = false);

            HWND GetHandle() const;

            void AddEventListener(EventCallback callback);

            void DestroyResources();

            static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

            HWND DoCreateStatusBar(HWND hwndParent, int idStatus, HINSTANCE
                hinst, int cParts);

            POINT GetMousePosition() const;

            void SetStatusBarText(std::wstring message, int part, int type);

            int WINAPI Create(HINSTANCE hInstance,
                int nCmdShow);

            friend DragAndDropTarget;
        private:
            void ResizeStatusBar();
            void RaiseEvent(const Event& evnt);

        private:
            HWND fHandleWindow;
            HWND fHandleStatusBar;
            int fStatusWindowParts;
            bool fIsFullScreen;
            WINDOWPLACEMENT fLastWindowPlacement;
            DragAndDropTarget* fDragAndDrop;
        public:
            EventCallbackCollection fListeners;
        };
    }
}
