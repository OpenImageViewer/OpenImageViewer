#include "Win32Window.h"
#include "MonitorInfo.h"
#include <tchar.h>
#include "Win32Helper.h"
#include <array>

namespace OIV
{
    namespace Win32
    {

        DWORD Win32WIndow::GetWindowStyles() const
        {
            DWORD currentStyles = GetWindowLong(fHandleWindow, GWL_STYLE);

            currentStyles &= (WS_MAXIMIZE | WS_VISIBLE);
            currentStyles |= WS_CLIPCHILDREN;

                
            if (fFullSceenState != FSS_Windowed || fShowBorders == false)
                return currentStyles;
            else
                return currentStyles
                | WS_BORDER
                | WS_DLGFRAME
                | WS_SYSMENU
                | WS_THICKFRAME
                | WS_MINIMIZEBOX
                | WS_MAXIMIZEBOX;
        }

        bool Win32WIndow::IsUnderMouseCursor() const
        {
            return WindowFromPoint(Win32Helper::GetMouseCursorPosition()) == GetHandle() == true;
        }

        HRESULT Win32WIndow::SendMessage(UINT msg, WPARAM wParam, LPARAM lparam)
        {
            return ::SendMessage(fHandleWindow, msg, wParam, lparam);
        }

        bool Win32WIndow::IsFullScreen() const
        {
            return fFullSceenState != FSS_Windowed;
        }

        void Win32WIndow::UpdateWindowStyles()
        {
            DWORD current = GetWindowStyles();
            if (current != fWindowStyles)
            {
                fWindowStyles = current;
                HWND hwnd = fHandleWindow;
                //Set styles
                SetWindowLong(hwnd, GWL_STYLE, fWindowStyles);

                //Refresh
                SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
        }

        void Win32WIndow::SetWindowed()
        {
            fFullSceenState = FSS_Windowed;
            UpdateWindowStyles();
            RestorePlacement();
        }



        void Win32WIndow::SavePlacement()
        {
            GetWindowPlacement(fHandleWindow, &fLastWindowPlacement);
        }


        void Win32WIndow::RestorePlacement()
        {
            SetWindowPlacement(fHandleWindow, &fLastWindowPlacement);
        }

        void Win32WIndow::RefreshWindow()
        {
            HandleResize();
        }

        void Win32WIndow::SetFullScreen(bool multiMonitor)
        {
            HWND hwnd = fHandleWindow;
            
            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(MonitorFromWindow(fHandleWindow, MONITOR_DEFAULTTOPRIMARY), &mi);
            RECT rect = mi.rcMonitor;

            if (fFullSceenState == FSS_Windowed)
                SavePlacement();

            if (multiMonitor)
            {
                MonitorInfo::GetSingleton().Refresh();
                rect = MonitorInfo::GetSingleton().getBoundingMonitorArea();
                fFullSceenState = FSS_MultiScreen;
            }
            else
                fFullSceenState = FSS_SingleScreen;

            UpdateWindowStyles();
            
            SetWindowPos(hwnd, HWND_TOP,
                rect.left, rect.top,
                rect.right - rect.left,
                rect.bottom - rect.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

            RefreshWindow();
        }

        void Win32WIndow::ToggleFullScreen(bool multiMonitor)
        {
            HWND hwnd = fHandleWindow;

            switch (fFullSceenState)
            {
            case FSS_Windowed:
                SetFullScreen(multiMonitor);
                break;
            case FSS_SingleScreen:
                if (multiMonitor == true)
                    SetFullScreen(multiMonitor);
                else
                    SetWindowed();
                break;
            case FSS_MultiScreen:
                SetWindowed();
                break;
            }
            
            ShowWindow(fHandleStatusBar, fFullSceenState == FSS_Windowed ? SW_SHOW : SW_HIDE);
        }

        HWND Win32WIndow::GetHandle() const
        {
            return fHandleWindow;
        }

        HWND Win32WIndow::GetHandleClient() const
        {   
            return fHandleClient;
        }

        void Win32WIndow::AddEventListener(EventCallback callback)
        {
            fListeners.push_back(callback);
        }

        void Win32WIndow::DestroyResources()
        {
            fDragAndDrop->Detach();
        }
#pragma region RawInput


        void Win32WIndow::FlushInput(bool calledFromIdleTimer)
        {
            
            uint64_t currentTime = static_cast<uint64_t>(fRawInputTimer.GetElapsedTimeInteger(LLUtils::StopWatch::TimeUnit::Milliseconds));

            if (currentTime - fRawInputLastEventDisptchTime > fRawInputInterval)
            {
                // Raise events and flush input

                EventRawInputMouseStateChanged rawInputEvent;
                rawInputEvent.window = this;
                rawInputEvent.DeltaX = static_cast<int16_t>(fMouseState.GetX());
                rawInputEvent.DeltaY = static_cast<int16_t>(fMouseState.GetY());
                rawInputEvent.DeltaWheel = static_cast<int16_t>(fMouseState.GetWheel());
                rawInputEvent.ChangedButtons = fMouseState.MoveButtonActions();
                RaiseEvent(rawInputEvent);
                fMouseState.Flush();

                fRawInputLastEventDisptchTime = currentTime;

                if (calledFromIdleTimer)
                    SetInputFlushTimer(false);
            }
            else
            {
                //skipped update, activate input flush timer
                SetInputFlushTimer(true);
            }
        }

     

        void Win32WIndow::HandleRawInput(RAWINPUT* event_raw_input)
        {
            if (event_raw_input->header.dwType == RIM_TYPEMOUSE)
                HandleRawInputMouse(event_raw_input->data.mouse);


            FlushInput(false);

        }


        

        void Win32WIndow::SetInputFlushTimer(bool enable)
        {
            if (fInputFlushTimerEnabled != enable)
            {
                fInputFlushTimerEnabled = enable;

                if (fInputFlushTimerEnabled)
                    SetTimer(fHandleWindow, cTimerIDRawInputFlush, 5, nullptr);
                else
                    KillTimer(fHandleWindow, cTimerIDRawInputFlush);
            }
        }

        void Win32WIndow::HandleRawInputMouse(const RAWMOUSE& mouse)
        {
            fMouseState.Update(mouse);
        }

        
#pragma endregion

        void Win32WIndow::Move(const int16_t delta_x, const int16_t delta_y)
        {
            RECT rect;
            GetWindowRect(fHandleWindow, &rect);
            MoveWindow(fHandleWindow, rect.left + delta_x, rect.top + delta_y, rect.right - rect.left, rect.bottom - rect.top, false);
        }

        LRESULT Win32WIndow::GetCorner(const POINTS& points) const
        {
            LRESULT corner = HTERROR;
            POINT p = { points.x , points.y };
            ScreenToClient(GetHandle(), &p);

            const LLUtils::PointI32 point = p;
            const LLUtils::PointI32 windowSize = GetWindowSize();
            using ArrayDouble4 = std::array<double, 4>;
            ArrayDouble4 distancesToCorners;

            distancesToCorners[0] = point.DistanceSquared({ 0,0 }); // Top left
            distancesToCorners[1] = point.DistanceSquared({ windowSize.x,0 }); // Top right
            distancesToCorners[2] = point.DistanceSquared(windowSize); // Botom right
            distancesToCorners[3] = point.DistanceSquared({ 0, windowSize.y }); // Bottom left
            ArrayDouble4::const_iterator it_min = std::min_element(distancesToCorners.begin(), distancesToCorners.end());

            int index = it_min - distancesToCorners.begin();
            switch (index)
            {
            case 0:
                corner = HTTOPLEFT;
                break;
            case 1:
                corner = HTTOPRIGHT;
                break;
            case 2:
                corner = HTBOTTOMRIGHT;
                break;
            case 3:
                corner = HTBOTTOMLEFT;
                break;
            default:
                throw std::logic_error("Unexepcted value");
            }

            return corner;
        }

        bool Win32WIndow::IsInFocus() const
        {
            return GetFocus() == fHandleWindow;
        }

        bool Win32WIndow::IsMouseCursorInClientRect() const
        {
            return PtInRect(&GetClientRectangle(), GetMousePosition()) == TRUE;
        }

        LRESULT Win32WIndow::ClientWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            switch (message)
            {
            case WM_NCHITTEST:
                return HTTRANSPARENT;
            case WM_ERASEBKGND:
                return 0;
            default:
                return DefWindowProc(hWnd, message, wParam, lParam);
            }
        }

        LLUtils::PointI32 Win32WIndow::GetWindowSize() const
        {
            RECT r;
            GetWindowRect(fHandleWindow, &r);
            return{ r.right - r.left, r.bottom - r.top };
        }

        void Win32WIndow::Show(bool show) const
        {
            ShowWindow(fHandleWindow, show == true ?SW_SHOW : SW_HIDE);
        }

        LRESULT Win32WIndow::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            Win32WIndow* window = reinterpret_cast<Win32WIndow*>(GetProp(hWnd, _T("windowClass")));

            if (window != nullptr && window->GetHandleClient() == hWnd)
                return window->ClientWndProc(hWnd, message, wParam, lParam);

            LRESULT retValue = 0;
            bool defaultProc = true;
            switch (message)
            {
            case WM_NCHITTEST:
            {
                if (true 
                    && window->fMouseState.IsCaptured(MouseState::Button::Left) == true 
                    && Win32Helper::IsKeyPressed(VK_MENU) == false 
                    && DefWindowProc(hWnd, message, wParam, lParam) == HTCLIENT
                    && window->IsFullScreen() == false
                    )
                {
                    defaultProc = false;
                    
                    if (Win32Helper::IsKeyPressed(VK_CONTROL) == true)
                        // Resize window.
                        retValue = window->GetCorner(*(POINTS*)&lParam);
                    else
                        // Drag window.
                        retValue = HTCAPTION;
                }
            }

            break;
            case WM_NCLBUTTONDBLCLK:
                //Enable double click only for the title bar.
                if ( DefWindowProc(hWnd, WM_NCHITTEST, wParam, lParam) != wParam)
                {
                    defaultProc = false;
                }
                break;
            case WM_TIMER:
                if (wParam == cTimerIDRawInputFlush)
                    window->FlushInput(true);
                break;

            case WM_CREATE:
            {
                CREATESTRUCT* s = (CREATESTRUCT*)lParam;
                if (SetProp(hWnd, _T("windowClass"), s->lpCreateParams) == 0)
                    std::exception("Unable to set window property");
            }
            case WM_ERASEBKGND:
                defaultProc = false;
                break;
            case WM_INPUT:
            {
                UINT dwSize;
                GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));

                std::unique_ptr<BYTE> lpb = std::unique_ptr<BYTE>(new BYTE[dwSize]);


                if (lpb == nullptr)
                    return 0;

                if (GetRawInputData((HRAWINPUT)lParam,
                    RID_INPUT,
                    lpb.get(),
                    &dwSize,
                    sizeof(RAWINPUTHEADER)) != dwSize)
                {
                    throw std::runtime_error("Unknown error");
                }
                window->HandleRawInput(reinterpret_cast<RAWINPUT*>(lpb.get()));
            }
            break;
            case WM_SIZE:
                if (window->GetHandle() == hWnd)
                    window->HandleResize();

                break;
            case WM_DESTROY:
                if (window->GetHandle() == hWnd)
                {
                    window->DestroyResources();
                    PostQuitMessage(0);
                }
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

            return (defaultProc == false ? retValue : DefWindowProc(hWnd, message, wParam, lParam));
        }

        HWND Win32WIndow::DoCreateStatusBar(HWND hwndParent, int idStatus, HINSTANCE hinst, int cParts)
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
                reinterpret_cast<HMENU>(idStatus), // child window identifier
                hinst, // handle to application instance
                nullptr); // no window creation data


            return hwndStatus;
        }

        POINT Win32WIndow::GetMousePosition() const
        {
            POINT clientMousePos;
            GetCursorPos(&clientMousePos);
            ScreenToClient(fHandleClient, &clientMousePos);
            return clientMousePos;
        }

        void Win32WIndow::SetStatusBarText(std::wstring message, int part, int type)
        {
            if (fHandleStatusBar != nullptr)
                ::SendMessage(fHandleStatusBar, SB_SETTEXT, MAKEWORD(part, type), reinterpret_cast<LPARAM>(message.c_str()));
        }

        int Win32WIndow::Create(HINSTANCE hInstance, int nCmdShow)
        {
            // The main window class name.
            static TCHAR szWindowClass[] = _T("OIV_WINDOW_CLASS");

            // The string that appears in the application's title bar.
            static TCHAR szTitle[] = _T("Open image viewer");
            WNDCLASSEX wcex;

            wcex.cbSize = sizeof(WNDCLASSEX);
            wcex.style = 0;// CS_HREDRAW | CS_VREDRAW | CS_;
            wcex.lpfnWndProc = WndProc;
            wcex.cbClsExtra = 0;
            wcex.cbWndExtra = 0;
            wcex.hInstance = hInstance;
            wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
            wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wcex.lpszMenuName = nullptr;
            wcex.lpszClassName = szWindowClass;
            wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));

            if (!RegisterClassEx(&wcex))
            {
                MessageBox(nullptr,
                    _T("Call to RegisterClassEx failed!"),
                    _T("Win32 Guided Tour"),
                    MB_OK);

                return 1;
            }
    

            fHandleWindow = CreateWindow(
                szWindowClass,
                szTitle,
                WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                CW_USEDEFAULT,
                CW_USEDEFAULT,
                1200,
                800,
                nullptr,
                nullptr,
                hInstance,
                this
            );


            if (!fHandleWindow)
            {
                MessageBox(nullptr,
                    _T("Call to CreateWindow failed!"),
                    _T("Win32 Guided Tour"),
                    MB_OK);

                return 1;
            }
            fHandleStatusBar = DoCreateStatusBar(fHandleWindow,  12, hInstance, 3);
            ResizeStatusBar();

            fWindowStylesClient = WS_CHILD;

            fHandleClient = CreateWindow(
                szWindowClass,
                szTitle,
                fWindowStylesClient,
                0,
                0,
                1200,
                800,
                fHandleWindow,
                nullptr,
                hInstance,
                this
            );


            ShowWindow(fHandleClient, SW_SHOW);

            ShowWindow(fHandleWindow,
                nCmdShow);
            UpdateWindow(fHandleWindow);

            fDragAndDrop = new DragAndDropTarget(*this);
            
            SetStatusBarText(_T("pixel: "), 0, SBT_NOBORDERS);
            SetStatusBarText(_T("File: "), 1, 0);

            RawInput::ResiterWindow(fHandleWindow);
            

            return 0;
        }

        void Win32WIndow::ResizeStatusBar()
        {
            if (fHandleStatusBar == nullptr)
                return;
            RECT rcClient;
            HLOCAL hloc;
            PINT paParts;
            int i, nWidth;
            if (GetClientRect(fHandleWindow, &rcClient) == 0)
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

        void Win32WIndow::RaiseEvent(const Event& evnt)
        {
            for (auto callback : fListeners)
                bool result = callback(&evnt);
        }

        SIZE Win32WIndow::GetClientSize() const
        {
            return Win32Helper::GetRectSize(GetClientRectangle());
        }

        RECT Win32WIndow::GetClientRectangle() const
        {
            RECT rect;
            GetClientRect(fHandleClient, &rect);
            return rect;
        }

       
        void Win32WIndow::HandleResize()
        {
            UpdateWindowStyles();

            RECT rect;
            GetClientRect(fHandleWindow, &rect);
            SIZE clientSize;
            clientSize.cx = rect.right - rect.left;
            clientSize.cy = rect.bottom - rect.top;


            if (fShowStatusBar && fFullSceenState == FullSceenState::FSS_Windowed)
            {
                RECT statusBarRect;
                ShowWindow(fHandleStatusBar,  SW_SHOW);
                GetWindowRect(fHandleStatusBar, &statusBarRect);
                clientSize.cy -= statusBarRect.bottom - statusBarRect.top;
                ResizeStatusBar();
            }
            else
            {
                ShowWindow(fHandleStatusBar, SW_HIDE);
            }

            SetWindowPos(fHandleClient, nullptr, 0, 0, clientSize.cx, clientSize.cy,0);
        }

        void Win32WIndow::ShowStatusBar(bool show)
        {
            fShowStatusBar = show;
            HandleResize();
        }

        void Win32WIndow::ShowBorders(bool show_borders)
        {
            fShowBorders = show_borders;
            fShowStatusBar = show_borders;
            RefreshWindow();
        }
    }
}
