#pragma once

#include <windows.h>
#include <Commctrl.h>
#include <vector>
#include <functional>
#include "DragAndDrop.h"
#include "../FullScreenState.h"


// Global variables
namespace OIV
{
    namespace Win32
    {
#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=nullptr; } }
        class Win32WIndow
        {
        public:


        public:
            Win32WIndow();
            void UpdateWindowStyles();
            void SetWindowed();
            void UpdateWindowPos();
            void SavePlacement();
            void RestorePlacement();
            void RefreshWindow();
            void SetFullScreen(bool multiMonitor);
            void ToggleFullScreen(bool multiMonitor = false);

            HWND GetHandle() const;
            HWND GetHandleClient() const;

            void AddEventListener(EventCallback callback);

            void DestroyResources();
            SIZE GetClientSize() const;
            void HandleResize();
            void ShowStatusBar(bool show);
            void ShowBorders(bool show_borders);
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
            
            DWORD fWindowStyles;
            DWORD fWindowStylesClient;
            HWND fHandleWindow;
            HWND fHandleClient;
            HWND fHandleStatusBar;
            int fStatusWindowParts;
            OIV::FullSceenState fFullSceenState;
            WINDOWPLACEMENT fLastWindowPlacement;
            
            DragAndDropTarget* fDragAndDrop;
            bool fShowStatusBar;
            bool fShowBorders;
        public:
            EventCallbackCollection fListeners;
        };
    }
}
