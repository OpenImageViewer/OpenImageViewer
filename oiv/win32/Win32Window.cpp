#include "Win32Window.h"
#include "MonitorInfo.h"
#include "../Logger.h"
#include <tchar.h>

namespace OIV
{
    namespace Win32
    {
        Win32WIndow::Win32WIndow() :
            fHandleWindow(nullptr)
            , fHandleStatusBar(nullptr)
            , fStatusWindowParts(4)
            , fIsFullScreen(false)
            , fLastWindowPlacement({ 0 })
            , fDragAndDrop(nullptr)
        {

        }


        void Win32WIndow::ToggleFullScreen(bool multiMonitor)
        {
            HWND hwnd = fHandleWindow;
            DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);

            if (fIsFullScreen == false)
            {
                MONITORINFO mi = { sizeof(mi) };
                if (GetWindowPlacement(hwnd, &fLastWindowPlacement) &&
                    GetMonitorInfo(MonitorFromWindow(hwnd,
                        MONITOR_DEFAULTTOPRIMARY), &mi))
                {
                    RECT rect = mi.rcMonitor;
                    if (multiMonitor)
                        rect = OIV::MonitorInfo::getSingleton().getBoundingMonitorArea();

                    SetWindowLong(hwnd, GWL_STYLE,
                        dwStyle & ~WS_OVERLAPPEDWINDOW & ~WS_CLIPCHILDREN);
                    SetWindowPos(hwnd, HWND_TOP,
                        rect.left, rect.top,
                        rect.right - rect.left,
                        rect.bottom - rect.top,
                        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                }
            }

            else
            {
                SetWindowLong(hwnd, GWL_STYLE,
                    dwStyle | WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN);
                SetWindowPlacement(hwnd, &fLastWindowPlacement);
                SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }

            fIsFullScreen = !fIsFullScreen;
            ShowWindow(fHandleStatusBar, fIsFullScreen ? SW_HIDE : SW_SHOW);
        }

        HWND Win32WIndow::GetHandle() const
        {
            return fHandleWindow;
        }

        void Win32WIndow::AddEventListener(EventCallback callback)
        {
            fListeners.push_back(callback);
        }

        void Win32WIndow::DestroyResources()
        {
            fDragAndDrop->Detach();
            SAFE_RELEASE(fDragAndDrop);
        }

        LRESULT Win32WIndow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            Win32WIndow* window = reinterpret_cast<Win32WIndow*>(GetProp(hWnd, _T("windowClass")));

            bool defaultProc = false;
            switch (message)
            {
            case WM_ERASEBKGND:
                break;
            case WM_SIZE:
                window->ResizeStatusBar();
                break;
            case WM_DESTROY:
                window->DestroyResources();

                PostQuitMessage(0);
                break;
            default:
                defaultProc = true;
                break;
            }

            if (window != nullptr)
            {
                EventWinMessage winEvent;
                winEvent.window = window;
                winEvent.message.hwnd = hWnd;
                winEvent.message.lParam = lParam;
                winEvent.message.wParam = wParam;
                winEvent.message.message = message;
                window->RaiseEvent(winEvent);
            
            }

            if (defaultProc)
                return DefWindowProc(hWnd, message, wParam, lParam);
            else
                return 0;
        }

        HWND Win32WIndow::DoCreateStatusBar(HWND hwndParent, int idStatus, HINSTANCE hinst, int cParts)
        {
            HWND hwndStatus;
            // Ensure that the common control DLL is loaded.
            InitCommonControls();

            // Create the status bar.
            hwndStatus = CreateWindowEx(
                0, // no extended styles
                STATUSCLASSNAME, // name of status bar class
                (PCTSTR)NULL, // no text when first created
                SBARS_SIZEGRIP | // includes a sizing grip
                WS_CHILD | WS_VISIBLE, // creates a visible child window
                0, 0, 0, 0, // ignores size and position
                hwndParent, // handle to parent window
                (HMENU)idStatus, // child window identifier
                hinst, // handle to application instance
                NULL); // no window creation data

                       // Get the coordinates of the parent window's client area.


            return hwndStatus;
        }

        POINT Win32WIndow::GetMousePosition() const
        {
            POINT clientMousePos;
            GetCursorPos(&clientMousePos);
            ScreenToClient(fHandleWindow, &clientMousePos);
            return clientMousePos;
        }

        void Win32WIndow::SetStatusBarText(std::wstring message, int part, int type)
        {
            if (fHandleStatusBar != NULL)
                SendMessage(fHandleStatusBar, SB_SETTEXT, MAKEWORD(part, type), reinterpret_cast<LPARAM>(message.c_str()));
        }

        int Win32WIndow::Create(HINSTANCE hInstance, int nCmdShow)
        {
            // The main window class name.
            static TCHAR szWindowClass[] = _T("OIV_WINDOW_CLASS");

            // The string that appears in the application's title bar.
            static TCHAR szTitle[] = _T("Open image viewer");
            WNDCLASSEX wcex;

            wcex.cbSize = sizeof(WNDCLASSEX);
            wcex.style = CS_HREDRAW | CS_VREDRAW;
            wcex.lpfnWndProc = WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = hInstance;
            wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
            wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
            wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wcex.lpszMenuName = NULL;
            wcex.lpszClassName = szWindowClass;
            wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

            if (!RegisterClassEx(&wcex))
            {
                MessageBox(NULL,
                    _T("Call to RegisterClassEx failed!"),
                    _T("Win32 Guided Tour"),
                    NULL);

                return 1;
            }

            HWND hWnd = CreateWindow(
                szWindowClass,
                szTitle,
                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                1200,
                800,
                NULL,
                NULL,
                hInstance,
                NULL
            );

            if (!hWnd)
            {
                MessageBox(NULL,
                    _T("Call to CreateWindow failed!"),
                    _T("Win32 Guided Tour"),
                    NULL);

                return 1;
            }
            if (SetProp(hWnd, _T("windowClass"), this) == 0)
                std::exception("Unable to set window property");

            fHandleWindow = hWnd;
            fHandleStatusBar = DoCreateStatusBar(hWnd, 12, hInstance, 3);

            ResizeStatusBar();
            ShowWindow(hWnd,
                nCmdShow);
            UpdateWindow(hWnd);

            fDragAndDrop = new DragAndDropTarget(*this);


            SetStatusBarText(_T("pixel: "), 0, SBT_NOBORDERS);
            SetStatusBarText(_T("File: "), 1, 0);

            return 0;
        }

        void Win32WIndow::ResizeStatusBar()
        {
            if (fHandleStatusBar == NULL)
                return;
            RECT rcClient;
            HLOCAL hloc;
            PINT paParts;
            int i, nWidth;
            if (GetClientRect(fHandleWindow, &rcClient) == 0)
                return;


            SetWindowPos(fHandleStatusBar, NULL, 0, 0, -1, -1, 0);
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
            SendMessage(fHandleStatusBar, SB_SETPARTS, (WPARAM)fStatusWindowParts, (LPARAM)
                paParts);

            // Free the array, and return.
            LocalUnlock(hloc);
            LocalFree(hloc);
        }

        void Win32WIndow::RaiseEvent(const Event& evnt)
        {
            for (auto callback : fListeners)
                bool result = callback(&evnt);
        }
    }
}