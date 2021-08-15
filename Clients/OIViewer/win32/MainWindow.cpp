#include "MainWindow.h"
#include <LLUtils/Buffer.h>
#include <Win32/BitmapHelper.h>
#include <Win32/Win32Helper.h>
#include "../resource.h"

namespace OIV
{
    namespace Win32
    {
        MainWindow::MainWindow()
        {
            AddEventListener(std::bind(&MainWindow::HandleWindwMessage, this, std::placeholders::_1));
        }


        void MainWindow::SetCursorType(CursorType type)
        {
            if (type != fCurrentCursorType && type >= CursorType::SystemDefault)
            {
                fCurrentCursorType = type;

                if (fCursorsInitialized == false)
                {
                    const LLUtils::native_string_type CursorsPath = LLUtils::StringUtility::ToNativeString(LLUtils::PlatformUtility::GetExeFolder()) + LLUTILS_TEXT("./Resources/Cursors/");

                    fCursors[0] = nullptr;
                    fCursors[static_cast<size_t>(CursorType::SystemDefault)] = LoadCursor(nullptr, IDC_ARROW);
                    fCursors[static_cast<size_t>(CursorType::East)] = LoadCursorFromFile((CursorsPath + LLUTILS_TEXT("arrow-E.cur")).c_str());
                    fCursors[static_cast<size_t>(CursorType::NorthEast)] = LoadCursorFromFile((CursorsPath + LLUTILS_TEXT("arrow-NE.cur")).c_str());
                    fCursors[static_cast<size_t>(CursorType::North)] = LoadCursorFromFile((CursorsPath + LLUTILS_TEXT("arrow-N.cur")).c_str());
                    fCursors[static_cast<size_t>(CursorType::NorthWest)] = LoadCursorFromFile((CursorsPath + LLUTILS_TEXT("arrow-NW.cur")).c_str());
                    fCursors[static_cast<size_t>(CursorType::West)] = LoadCursorFromFile((CursorsPath + LLUTILS_TEXT("arrow-W.cur")).c_str());
                    fCursors[static_cast<size_t>(CursorType::SouthWest)] = LoadCursorFromFile((CursorsPath + LLUTILS_TEXT("arrow-SW.cur")).c_str());
                    fCursors[static_cast<size_t>(CursorType::South)] = LoadCursorFromFile((CursorsPath + LLUTILS_TEXT("arrow-S.cur")).c_str());
                    fCursors[static_cast<size_t>(CursorType::SouthEast)] = LoadCursorFromFile((CursorsPath + LLUTILS_TEXT("arrow-SE.cur")).c_str());
                    fCursors[static_cast<size_t>(CursorType::SizeAll)] = LoadCursorFromFile((CursorsPath + LLUTILS_TEXT("arrow-C.cur")).c_str());



                    fCursorsInitialized = true;
                }
                fCurrentCursorType = type;
                SetMouseCursor(fCurrentCursorType == CursorType::SystemDefault ? nullptr : fCursors[static_cast<int>(fCurrentCursorType)]);
            }
        }

        void MainWindow::OnCreate()
        {
            //fHandleStatusBar = DoCreateStatusBar(GetHandle(), 12, GetModuleHandle(nullptr), 3);
            //ResizeStatusBar();

            fCanvasWindow.Create();
            fCanvasWindow.SetParent(this);
            fCanvasWindow.SetVisible(true);
            fCanvasWindow.SetTransparent(true);

          
            SetWindowIcon(::Win32::MakeIntResource(IDI_APP_ICON));

           
       

            //SetStatusBarText(L"pixel: ", 0, SBT_NOBORDERS);
            //SetStatusBarText(L"File: ", 1, 0);

        }



        HWND MainWindow::DoCreateStatusBar(HWND hwndParent, uint32_t idStatus, HINSTANCE hinst, [[maybe_unused]] uint32_t cParts)
        {
            HWND hwndStatus;

            // Create the status bar.
            hwndStatus = CreateWindowEx(
                0, // no extended styles
                STATUSCLASSNAME, // name of status bar class
                nullptr, // no text when first created
                SBARS_SIZEGRIP | // includes a sizing grip
                WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, // creates a visible child window
                0, 0, 0, 0, // ignores size and position
                hwndParent, // handle to parent window
                reinterpret_cast<HMENU>(static_cast<size_t>(idStatus)), // child window identifier
                hinst, // handle to application instance
                nullptr); // no window creation data


            return hwndStatus;
        }


        void MainWindow::SetStatusBarText(std::wstring message, int part, int type)
        {
            if (fHandleStatusBar != nullptr)
                ::SendMessage(fHandleStatusBar, SB_SETTEXT, MAKEWORD(part, type), reinterpret_cast<LPARAM>(message.c_str()));
        }


        void MainWindow::ResizeStatusBar()
        {
            if (fHandleStatusBar == nullptr)
                return;
            RECT rcClient;
            HLOCAL hloc;
            PINT paParts;
            int i, nWidth;
            if (GetClientRect(GetHandle(), &rcClient) == 0)
                return;


            SetWindowPos(fHandleStatusBar, nullptr, 0, 0, -1, -1, 0);
            // Allocate an array for holding the right edge coordinates.
            hloc = LocalAlloc(LHND, sizeof(int) * fStatusWindowParts);
            paParts = (PINT)LocalLock(hloc);

            // Calculate the right edge coordinate for each part, and
            // copy the coordinates to the array.
            nWidth = rcClient.right / fStatusWindowParts;
            int rightEdge = nWidth;
            for (i = 0; i < fStatusWindowParts; i++)
            {
                paParts[i] = rightEdge;
                rightEdge += nWidth;
            }

            // Tell the status bar to create the window parts.
            ::SendMessage(fHandleStatusBar, SB_SETPARTS, (WPARAM)fStatusWindowParts, (LPARAM)
                paParts);

            // Free the array, and return.
            LocalUnlock(hloc);
            LocalFree(hloc);
        }



        bool MainWindow::GetShowImageControl() const
        {
            return fShowImageControl;
        }

        bool MainWindow::GetShowStatusBar() const
        {
            using namespace ::Win32;
            // show status bar if explicity not visible and caption is visible
            return fShowStatusBar == true &&
                ((GetWindowStyles() & (WindowStyle::Caption | WindowStyle::CloseButton | WindowStyle::MinimizeButton | WindowStyle::MaximizeButton)) != WindowStyle::NoStyle);
        }

        void MainWindow::HandleResize()
        {
            RECT rect;
            GetClientRect(GetHandle(), &rect);
            SIZE clientSize;
            const int ImageListWidth = 200;
            const bool isImageControlVisible = GetShowImageControl();
            clientSize.cx = rect.right - rect.left - (isImageControlVisible ?  ImageListWidth : 0 );
            clientSize.cy = rect.bottom - rect.top;


            if (GetShowStatusBar() && GetFullScreenState() == ::Win32::FullSceenState::Windowed)
            {
                RECT statusBarRect;
                ShowWindow(fHandleStatusBar, SW_SHOW);
                GetWindowRect(fHandleStatusBar, &statusBarRect);
                clientSize.cy -= statusBarRect.bottom - statusBarRect.top;
                ResizeStatusBar();
            }
            else
            {
                ShowWindow(fHandleStatusBar, SW_HIDE);
            }

            SetWindowPos(fCanvasWindow.GetHandle(), nullptr, 0, 0, clientSize.cx, clientSize.cy, 0);

            if (fImageControl.GetHandle() != nullptr)
            {
                fImageControl.SetVisible(isImageControlVisible);

                if (isImageControlVisible)
                    SetWindowPos(fImageControl.GetHandle(), nullptr, clientSize.cx, 0, ImageListWidth, clientSize.cy, SWP_NOACTIVATE | SWP_NOZORDER);
            }

            ShowWindow(fHandleStatusBar, GetFullScreenState() == ::Win32::FullSceenState::Windowed ? SW_SHOW : SW_HIDE);
        }

        void MainWindow::ShowStatusBar(bool show)
        {
            if (show != fShowStatusBar)
            {
                fShowStatusBar = show;
                HandleResize();
            }
        }

        void MainWindow::SetShowImageControl(bool show)
        {
            if (show != fShowImageControl)
            {
                fShowImageControl = show;
                if (fImageControl.GetHandle() == nullptr)
				{
                    fImageControl.Create();
		            fImageControl.SetParent(this);
				}
                HandleResize();
            }
        }


        HWND MainWindow::GetCanvasHandle() const
        {
            return fCanvasWindow.GetHandle();
        }



        SIZE MainWindow::GetCanvasSize() const
        {
            RECT rect;
            GetClientRect(GetCanvasHandle(), &rect);
            return ::Win32::Win32Helper::GetRectSize(rect);
        }



        LRESULT MainWindow::HandleWindwMessage(const ::Win32::Event* evnt1)
        {
            using namespace ::Win32;
            const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);
            if (evnt == nullptr)
                return 0;

            const WinMessage & message = evnt->message;

            LRESULT retValue = 0;
            switch (message.message)
            {
            case WM_CREATE:
                OnCreate();
                break;

            case WM_SIZE:
                HandleResize();
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            case WM_ACTIVATE:
                if (message.wParam != WA_INACTIVE)
                 SetIsTrayWindow(false);
                break;
            }
            return retValue;
        }

        void MainWindow::SetIsTrayWindow(bool isTrayWindow)
        {
            ::SetProp(GetHandle(), LLUTILS_TEXT("isTrayWindow"), isTrayWindow ?  reinterpret_cast<HANDLE>(1) : nullptr);
        }
        
        bool MainWindow::GetIsTrayWindow(HWND hwnd)
        {
            return ::GetProp(hwnd, LLUTILS_TEXT("isTrayWindow")) != nullptr;
       }
    }
}