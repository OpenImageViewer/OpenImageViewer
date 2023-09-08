#pragma once
#include <Win32/Win32Window.h>
#include "ImageControl.h"
namespace OIV
{
    namespace Win32
    {
        class MainWindow : public ::Win32::Win32Window
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
            void SetStatusBarText(std::wstring message, int part, int type);
            void SetIsTrayWindow(bool isTrayWindow);
            static bool GetIsTrayWindow(HWND hwnd);


        private: // methods
            void HandleResize();
            //void ResizeStatusBar();
            LRESULT HandleWindwMessage(const ::Win32::Event* evnt1);
            HWND DoCreateStatusBar(HWND hwndParent, uint32_t idStatus, HINSTANCE hinst, uint32_t cParts);
            void OnCreate();

        private: // member fields
            Win32Window fCanvasWindow;
            HWND fHandleStatusBar = nullptr;
            //int fStatusWindowParts = 6;
            bool fShowStatusBar = true;
            bool fShowImageControl = false;
            CursorType fCurrentCursorType = CursorType::SystemDefault;
            std::array<HCURSOR, static_cast<int>(CursorType::Count)> fCursors{};
            bool fCursorsInitialized = false;
            ImageControl fImageControl;
        };
    }
}