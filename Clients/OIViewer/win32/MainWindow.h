#pragma once
#include "Win32Window.h"
#include "ImageControl.h"
namespace OIV
{
    namespace Win32
    {
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
            const RawInputMouseWindow& GetMouseState() const { return fMouseState; }
            bool GetShowImageControl() const;
            bool GetShowStatusBar() const;
            SIZE GetCanvasSize() const;
            HWND GetCanvasHandle() const;


        public: // mutating methods
            ImageControl& GetImageControl() { return fImageControl; }
            void SetCursorType(CursorType type);
            void ShowStatusBar(bool show);
            void SetShowImageControl(bool show);
            Win32Window& GetCanvasWindow() { return fCanvasWindow; }
            void FlushInput(bool calledFromIdleTimer);
            void HandleRawInput(RAWINPUT* event_raw_input);
            void SetInputFlushTimer(bool enable);
            void SetStatusBarText(std::wstring message, int part, int type);
            void SetIsTrayWindow(bool isTrayWindow);
            static bool GetIsTrayWindow(HWND hwnd);


        private: // methods
            void HandleResize();
            void HandleRawInputMouse(const RAWMOUSE& mouse);
            void HandleRawInputKeyboard(const RAWKEYBOARD& keyboard);
            void ResizeStatusBar();
            LRESULT HandleWindwMessage(const Win32::Event* evnt1);
            HWND DoCreateStatusBar(HWND hwndParent, uint32_t idStatus, HINSTANCE hinst, uint32_t cParts);
            void OnCreate();

        private: // member fields
            Win32Window fCanvasWindow;
            RawInputMouseWindow fMouseState = RawInputMouseWindow(static_cast<Win32Window*>(this));
            HWND fHandleStatusBar = nullptr;
            int fStatusWindowParts = 6;
            bool fShowStatusBar = true;
            bool fShowImageControl = false;
            bool fInputFlushTimerEnabled = false;
            static const int cTimerIDRawInputFlush = 2500;
            uint16_t fRawInputInterval = 0;
            LLUtils::StopWatch fRawInputTimer = (true);
            uint64_t fRawInputLastEventDisptchTime = 0;
            CursorType fCurrentCursorType = CursorType::SystemDefault;
            std::array<HCURSOR, static_cast<int>(CursorType::Count)> fCursors;
            bool fCursorsInitialized = false;
            ImageControl fImageControl;
        };
    }
}