#pragma once

#include <windows.h>
#include <Commctrl.h>
#include <vector>
#include <functional>
#include "DragAndDrop.h"
#include "../FullScreenState.h"
#include "RawInput/RawInputMouseWindow.h"
#include "StopWatch.h"
#include <wrl/client.h>

namespace OIV
{
    namespace Win32
    {
        class Win32WIndow
        {
        public: // constant methods
            DWORD GetWindowStyles() const;
            bool IsFullScreen() const;
            HWND GetHandle() const;
            HWND GetHandleClient() const;
            SIZE GetClientSize() const;
            RECT GetClientRectangle() const;
            bool IsInFocus() const;
            bool IsMouseCursorInClientRect() const;
            LRESULT GetCorner(const POINTS& tag_points) const;
            LLUtils::PointI32 GetWindowSize() const;
            void Show(bool show) const;
            POINT GetMousePosition() const;
            const RawInputMouseWindow& GetMouseState() const { return fMouseState; }
            bool IsUnderMouseCursor() const;
            FullSceenState GetFullScreenState() const { return fFullSceenState; }

        public: // methods
            HRESULT SendMessage(UINT msg, WPARAM wParam, LPARAM lparam);
            void RefreshWindow();
            void ToggleFullScreen(bool multiMonitor = false);
            void AddEventListener(EventCallback callback);
            void DestroyResources();
            void HandleResize();
            void ShowStatusBar(bool show);
            void ShowBorders(bool show_borders);
            void FlushInput(bool calledFromIdleTimer);
            void Win32WIndow::HandleRawInput(RAWINPUT* event_raw_input);
            void SetInputFlushTimer(bool enable);
            
            void Move(const int16_t delta_x, const int16_t delta_y);
            static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
            HWND DoCreateStatusBar(HWND hwndParent, int idStatus, HINSTANCE hinst, int cParts);
            void SetStatusBarText(std::wstring message, int part, int type);
            int WINAPI Create(HINSTANCE hInstance, int nCmdShow);

            friend DragAndDropTarget;
        private: // methods
            void HandleRawInputMouse(const RAWMOUSE& mouse);
            void HandleRawInputKeyboard(const RAWKEYBOARD& keyboard);

            void UpdateWindowStyles();
            void SavePlacement();
            void RestorePlacement();
            void SetWindowed();
            void SetFullScreen(bool multiMonitor);
            LRESULT ClientWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
            void ResizeStatusBar();
            bool RaiseEvent(const Event& evnt);

        private: // member fields

            DWORD fWindowStyles = 0;
            DWORD fWindowStylesClient = 0;
            HWND fHandleWindow = nullptr;
            HWND fHandleClient = nullptr;
            HWND fHandleStatusBar = nullptr;
            int fStatusWindowParts = 6;
            FullSceenState fFullSceenState = FullSceenState::Windowed;
            WINDOWPLACEMENT fLastWindowPlacement = { 0 };
            Microsoft::WRL::ComPtr<DragAndDropTarget> fDragAndDrop;
            bool fShowStatusBar = true;
            bool fShowBorders = true;
            RawInputMouseWindow fMouseState = RawInputMouseWindow(this);
            bool fInputFlushTimerEnabled = false;
            static const int cTimerIDRawInputFlush = 2500;
            uint16_t fRawInputInterval = 5;
            LLUtils::StopWatch fRawInputTimer = (true);
            uint64_t fRawInputLastEventDisptchTime = 0;;
            EventCallbackCollection fListeners;
        };
    }
}