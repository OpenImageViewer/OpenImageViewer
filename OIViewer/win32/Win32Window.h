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
#include <BitFlags.h>
#include <Templates.h>
#include <Color.h>

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
        
        enum class WindowStyle : uint32_t
        {
              NoStyle        = 0 << 0 // WS_CAPTION
            , Caption        = 1 << 0 // WS_CAPTION
            , CloseButton    = 1 << 1 // WS_SYSMENU
            , ResizableBorder= 1 << 2 // WS_SIZEBOX
            , MinimizeButton = 1 << 3 // WS_MINIMIZEBOX
            , MaximizeButton = 1 << 4 // WS_MAXIMIZEBOX;
            , ChildWindow    = 1 << 5 // WS_CHILD
            , All            = LLUtils::GetMaxBitsMask<uint32_t>()
        };

        using WindowStyleFlags = LLUtils::BitFlags<WindowStyle>;
        
        class Win32Window;

        using VecChildWindows = std::vector<Win32Window*>;
        
        class Win32Window
        {
        public:
            // const methods
            void SetFocused() const;
            HWND GetHandle() const;
            bool IsInFocus() const;
            bool IsMouseCursorInClientRect() const;
            POINT GetMousePosition() const;
            bool IsFullScreen() const;
            SIZE GetClientSize() const;
            RECT GetClientRectangle() const;
            LRESULT GetCorner(const POINTS& tag_points) const;
            LLUtils::PointI32 GetWindowSize() const;
            FullSceenState GetFullScreenState() const { return fFullSceenState; } ;
            bool IsUnderMouseCursor() const;
            bool GetEraseBackground() const { return fEraseBackground; }
            DoubleClickMode GetDoubleClickMode() const { return fDoubleClickMode; }
            bool GetEnableMenuChar() const { return fEnableMenuChar; }
            HCURSOR GetMouseCursor() const { return fMouseCursor; }
            LockMouseToWindowMode GetLockMouseToWindowMode() const { return fLockMouseToWindowMode; }
            bool GetVisible() const  { return fVisible; }
            WindowStyleFlags GetWindowStyles() const {return fWindowStyles;}
            bool GetTransparent() const { return fIsTransparent; }
            Win32Window* GetParent() const { return fParent; }
            bool GetAlwaysOnTop() const;
            
            
            virtual ~Win32Window() {};

        public: // mutating methods
            int WINAPI Create();
            LRESULT SendMessage(UINT msg, WPARAM wParam, LPARAM lparam);
            void AddEventListener(EventCallback callback);
            void SetMenuChar(bool enabled);
            void SetPosition(int32_t x, int32_t y);
            void SetSize(uint32_t width, uint32_t height);
            void SetPlacement(int32_t x, int32_t y, uint32_t width, uint32_t height);
            void ToggleFullScreen(bool multiMonitor = false);
            void Move(const int16_t delta_x, const int16_t delta_y);
            void SetMouseCursor(HCURSOR cursor);
            void SetEraseBackground(bool eraseBackground) {fEraseBackground = eraseBackground;}
			void SetBackgroundColor(const LLUtils::Color color);
            void SetDoubleClickMode(DoubleClickMode doubleClickMode) { fDoubleClickMode = doubleClickMode; }
            void EnableDragAndDrop(bool enable);
            void SetLockMouseToWindowMode(LockMouseToWindowMode mode);
            void SetVisible(bool visible);
            void SetWindowStyles(WindowStyle styles, bool enable);
            void SetParent(Win32Window* parent);
            void SetTransparent(bool transparent) {fIsTransparent = transparent;}
            void SetAlwaysOnTop(bool alwaysOnTop);
			void SetTitle(const std::wstring& title);


        protected:
            bool RaiseEvent(const Event& evnt);
            DWORD ComposeWindowStyles() const;

        private: // const methods:

        private: //methods:
            static LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
            LRESULT WindowProc(const WinMessage& message);
            void UpdateWindowStyles();
            void SavePlacement();
            void RestorePlacement();
            void SetWindowed();
            void SetFullScreen(bool multiMonitor);
            void NotifyRemovedForRelatedWindows();
            void AddChild(Win32Window* child);
            void RemoveChild(Win32Window* child);
            void DestroyResources();
        private:
            Win32Window* fParent = nullptr;
            VecChildWindows fChildren;
            HCURSOR fMouseCursor = nullptr;
            HWND fHandleWindow = nullptr;
            EventCallbackCollection fListeners;
            bool fEnableMenuChar = true;
            FullSceenState fFullSceenState = FullSceenState::Windowed;
            WINDOWPLACEMENT fLastWindowPlacement = { 0 };
            Microsoft::WRL::ComPtr<DragAndDropTarget> fDragAndDrop;
            bool fEraseBackground = true;
            DoubleClickMode fDoubleClickMode = DoubleClickMode::NotSet;
            friend DragAndDropTarget;
            LockMouseToWindowMode fLockMouseToWindowMode = LockMouseToWindowMode::NoLock;
            bool fVisible = false;
            bool fIsMaximized = false;
            WindowStyleFlags fWindowStyles = WindowStyle::NoStyle;
            bool fIsTransparent = false;
            bool fAlwaysOnTop = false;
			LLUtils::Color fBackgroundColor = LLUtils::Color(255_u8, 255, 255);
			HBRUSH fBackgroundCachedBrush = CreateSolidBrush(fBackgroundColor.colorValue);
        };
    }
}

