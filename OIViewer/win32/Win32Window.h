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
#include "Win32Common.h"

namespace OIV
{
    namespace Win32
    {
        enum class DoubleClickMode
        {
              NotSet // use the behaviour defined by the window
            , Default //use the bevour defined by a default window.
            , EntireWindow // Double click allowed on the entire window
            , ClientArea // double click allows only on the client area.
            , NonClientArea // e.g. title
        };

        enum class LockMouseToWindowMode
        {
              NoLock
            , LockResize
            , LockMove // e.g. title
        };

        class Win32Window
        {
        public:
            // mutating methods
            HWND GetHandle() const;
            bool IsInFocus() const;
            bool IsMouseCursorInClientRect() const;
            POINT GetMousePosition() const;
            DWORD GetWindowStyles() const;
            bool IsFullScreen() const;
            SIZE GetClientSize() const;
            RECT GetClientRectangle() const;
            LRESULT GetCorner(const POINTS& tag_points) const;
            LLUtils::PointI32 GetWindowSize() const;
            void Show(bool show) const;
            FullSceenState GetFullScreenState() const { return fFullSceenState; } ;
            bool IsUnderMouseCursor() const;
            bool GetEraseBackground() const { return fEraseBackground; }
            DoubleClickMode GetDoubleClickMode() const { return fDoubleClickMode; }
            bool GetEnableMenuChar() const { return fEnableMenuChar; }
            HCURSOR GetMouseCursor() const { return fMouseCursor; }
            LockMouseToWindowMode GetLockMouseToWindowMode() const { return fLockMouseToWindowMode; }
            
            virtual ~Win32Window() {};

        public: // mutating methods
            int WINAPI Create(HINSTANCE hInstance, int nCmdShow);
            HRESULT SendMessage(UINT msg, WPARAM wParam, LPARAM lparam);
            void AddEventListener(EventCallback callback);
            void SetMenuChar(bool enabled);
            void ToggleFullScreen(bool multiMonitor = false);
            bool GetShowBorders() const;
            void Move(const int16_t delta_x, const int16_t delta_y);
            void SetMouseCursor(HCURSOR cursor);
            void SetEraseBackground(bool eraseBackground) {fEraseBackground = eraseBackground;}
            void SetDoubleClickMode(DoubleClickMode doubleClickMode) { fDoubleClickMode = doubleClickMode; }
            void EnableDragAndDrop(bool enable);
            void SetLockMouseToWindowMode(LockMouseToWindowMode mode);
            void ShowBorders(bool show_borders);

        protected:
            bool RaiseEvent(const Event& evnt);
        private: //methods:
            static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
            LRESULT WindowProc(const WinMessage& message);
            void UpdateWindowStyles();
            void SavePlacement();
            void RestorePlacement();
            void SetWindowed();
            void SetFullScreen(bool multiMonitor);

            void DestroyResources();
        private:
            HCURSOR fMouseCursor = nullptr;
            HWND fHandleWindow = nullptr;
            EventCallbackCollection fListeners;
            bool fEnableMenuChar = true;
            DWORD fWindowStyles = 0;
            FullSceenState fFullSceenState = FullSceenState::Windowed;
            WINDOWPLACEMENT fLastWindowPlacement = { 0 };
            Microsoft::WRL::ComPtr<DragAndDropTarget> fDragAndDrop;
            bool fEraseBackground = true;
            DoubleClickMode fDoubleClickMode = DoubleClickMode::NotSet;
            friend DragAndDropTarget;
            bool fShowBorders = true;
            LockMouseToWindowMode fLockMouseToWindowMode = LockMouseToWindowMode::NoLock;
        };


        class MainWindow : public Win32Window
        {
        public: // Types
            enum class CursorType
            {
                SystemDefault
                , Arrow
                , East
                , NorthEast
                , North
                , NorthWest
                , West
                , SouthWest
                , South
                , SouthEast
                , SizeAll
                , Count
            };

            MainWindow();
        public: // constant methods
            HWND GetHandleClient() const;
            const Win32::RawInputMouseWindow& GetMouseState() const { return fMouseState; }
            bool GetShowStatusBar() const;
            SIZE GetClientSize() const;

        public: // mutating methods
            void SetCursorType(CursorType type);
            void HandleResize();
            void ShowStatusBar(bool show);
            void FlushInput(bool calledFromIdleTimer);
            void HandleRawInput(RAWINPUT* event_raw_input);
            void SetInputFlushTimer(bool enable);
            void SetStatusBarText(std::wstring message, int part, int type);
            

            
        private: // methods
            void HandleRawInputMouse(const RAWMOUSE& mouse);
            void HandleRawInputKeyboard(const RAWKEYBOARD& keyboard);
            static LRESULT ClientWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
            void ResizeStatusBar();
            LRESULT HandleWindwMessage(const Win32::Event* evnt1);
            HWND DoCreateStatusBar(HWND hwndParent, uint32_t idStatus, HINSTANCE hinst, uint32_t cParts);
            void OnCreate();
            
        private: // member fields
            //Win32Window fClientWindow;
            RawInputMouseWindow fMouseState = RawInputMouseWindow(static_cast<Win32Window*>(this));
            DWORD fWindowStylesClient = 0;
            HWND fHandleClient = nullptr;
            HWND fHandleStatusBar = nullptr;
            int fStatusWindowParts = 6;
            bool fShowStatusBar = true;
            bool fInputFlushTimerEnabled = false;
            static const int cTimerIDRawInputFlush = 2500;
            uint16_t fRawInputInterval = 0;
            LLUtils::StopWatch fRawInputTimer = (true);
            uint64_t fRawInputLastEventDisptchTime = 0;;
            CursorType fCurrentCursorType = CursorType::SystemDefault;
            std::array<HCURSOR, static_cast<int>(CursorType::Count)> fCursors;
            bool fCursorsInitialized = false;
        };
    }
}