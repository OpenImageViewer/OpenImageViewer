#include "Win32Window.h"
#include "MonitorInfo.h"
#include <tchar.h>
#include "Win32Helper.h"
#include <array>
#include "../resource.h"
#include "Buffer.h"
#include <assert.h>

namespace OIV
{
    namespace Win32
    {
        

        void Win32Window::AddChild(Win32Window* child)
        {
            fChildren.push_back(child);
        }
        void Win32Window::RemoveChild(Win32Window* child)
        {
            auto it = std::find(fChildren.begin(), fChildren.end(), child);
            fChildren.erase(it);
        }

        void Win32Window::NotifyRemovedForRelatedWindows()
        {
            decltype(fChildren) childrenCopy = fChildren;
            for (Win32Window* child : childrenCopy)
                child->SetParent(nullptr);

            SetParent(nullptr);
        }


        void Win32Window::SetParent(Win32Window* parent)
        {
            if (parent != fParent)
            {
                if (fParent != nullptr)
                    fParent->RemoveChild(this);

                
                fParent = parent;
                if (fParent != nullptr)
                    fParent->AddChild(this);

                SetWindowStyles(WindowStyle::ChildWindow, parent != nullptr);
               ::SetParent(GetHandle(), fParent != nullptr ? parent->GetHandle() : nullptr);

            }
        }
        int Win32Window::Create()
        {
            HINSTANCE hInstance = GetModuleHandle(nullptr);
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
            wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
            wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
            wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
            wcex.lpszMenuName = nullptr;
            wcex.lpszClassName = szWindowClass;
            wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_APPLICATION));
            static bool classRegistered = false;
            if (classRegistered == false)
            {

                if (RegisterClassEx(&wcex) == false)
                {
                    MessageBox(nullptr,
                        _T("Call to RegisterClassEx failed!"),
                        _T("Win32 Guided Tour"),
                        MB_OK);

                    return 1;
                }

                classRegistered = true;
            }

            // fHandleWindow is set in WM_CREATE


            HWND handleWindow =

                CreateWindow(
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


            assert("Wrong window handle" && fHandleWindow == handleWindow);


            if (!fHandleWindow)
            {
                MessageBox(nullptr,
                    _T("Call to CreateWindow failed!"),
                    _T("Win32 Guided Tour"),
                    MB_OK);

                return 1;
            }

            //MSG msg;
            //BOOL bRet;

            //while ((bRet = PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) != 0)
            //{
            //    if (bRet == -1)
            //    {
            //        // handle the error and possibly exit
            //    }
            //    else
            //    {
            //        TranslateMessage(&msg);
            //        DispatchMessage(&msg);
            //    }
            //}



            return 0;
        }

        void Win32Window::SetLockMouseToWindowMode(LockMouseToWindowMode mode)
        {
            fLockMouseToWindowMode = mode;
        }

        void Win32Window::SetMenuChar(bool enabled)
        {
            fEnableMenuChar = enabled;
        }

        HWND Win32Window::GetHandle() const
        {
            return fHandleWindow;
        }

        void Win32Window::AddEventListener(EventCallback callback)
        {
            fListeners.push_back(callback);
        }

        bool Win32Window::IsInFocus() const
        {
            return GetFocus() == fHandleWindow;
        }

        bool Win32Window::IsMouseCursorInClientRect() const
        {
            return PtInRect(&GetClientRectangle(), GetMousePosition()) == TRUE;
        }

        void Win32Window::SetWindowed()
        {
            fFullSceenState = FullSceenState::Windowed;
            UpdateWindowStyles();
            RestorePlacement();
        }

        void Win32Window::SavePlacement()
        {
            GetWindowPlacement(fHandleWindow, &fLastWindowPlacement);
        }


        void Win32Window::RestorePlacement()
        {
            SetWindowPlacement(fHandleWindow, &fLastWindowPlacement);
        }




        DWORD Win32Window::ComposeWindowStyles() const
        {
            DWORD currentStyles = 0
                | (WS_CLIPCHILDREN | WS_CLIPSIBLINGS)
                | (GetVisible() ? WS_VISIBLE : 0)
                ;


            if (GetFullScreenState() == FullSceenState::Windowed)
            {
                currentStyles |= 0
                    | (fWindowStyles.test(WindowStyle::ChildWindow) ? WS_CHILD : 0)
                    | (fWindowStyles.test(WindowStyle::Caption) ? WS_CAPTION : 0)
                    | (fWindowStyles.test(WindowStyle::CloseButton) ? WS_SYSMENU | WS_CAPTION : 0)
                    | (fWindowStyles.test(WindowStyle::MinimizeButton)  ? WS_SYSMENU | WS_CAPTION | WS_MINIMIZEBOX : 0)
                    | (fWindowStyles.test(WindowStyle::MaximizeButton) ? WS_SYSMENU | WS_CAPTION | WS_MAXIMIZEBOX : 0)
                    | (fWindowStyles.test(WindowStyle::ResizableBorder) ? WS_SIZEBOX : 0)
                    ;
            }

            return currentStyles;

        }

        bool Win32Window::IsUnderMouseCursor() const
        {
            return WindowFromPoint(Win32Helper::GetMouseCursorPosition()) == GetHandle() == true;
        }

        void Win32Window::SetWindowStyles(WindowStyle styles, bool enable)
        {
            WindowStyleFlags oldStyles = fWindowStyles;
            if (enable)
                fWindowStyles.set(styles);
            else
                fWindowStyles.clear(styles);

            if (oldStyles != fWindowStyles)
                UpdateWindowStyles();



        }

        void Win32Window::SetMouseCursor(HCURSOR cursor)
        {
            if (fMouseCursor != cursor)
            {
                fMouseCursor = cursor;
            }
        }

        void Win32Window::EnableDragAndDrop(bool enable)
        {
            bool const enabled = fDragAndDrop != nullptr;

            if (enabled != enable)
            {

                switch (enable)
                {
                case false:
                    fDragAndDrop->Detach();
                    fDragAndDrop.Reset();
                    break;
                case true:
                    fDragAndDrop = new DragAndDropTarget(*this);
                    break;
                }
            }
        }

        HRESULT Win32Window::SendMessage(UINT msg, WPARAM wParam, LPARAM lparam)
        {
            return ::SendMessage(fHandleWindow, msg, wParam, lparam);
        }

        bool Win32Window::IsFullScreen() const
        {
            return fFullSceenState != FullSceenState::Windowed;
        }

        void Win32Window::UpdateWindowStyles()
        {
            //Set styles
            SetWindowLong(GetHandle(), GWL_STYLE, ComposeWindowStyles());

            //Refresh frame
            SetWindowPos(GetHandle(), nullptr, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }


        void Win32Window::SetFullScreen(bool multiMonitor)
        {
            HWND hwnd = fHandleWindow;

            MONITORINFO mi = { sizeof(mi) };
            GetMonitorInfo(MonitorFromWindow(fHandleWindow, MONITOR_DEFAULTTOPRIMARY), &mi);
            RECT rect = mi.rcMonitor;

            if (fFullSceenState == FullSceenState::Windowed)
                SavePlacement();

            if (multiMonitor)
            {
                MonitorInfo::GetSingleton().Refresh();
                rect = MonitorInfo::GetSingleton().getBoundingMonitorArea();
                fFullSceenState = FullSceenState::MultiScreen;
            }
            else
                fFullSceenState = FullSceenState::SingleScreen;

            UpdateWindowStyles();

            SetWindowPos(hwnd, HWND_TOP,
                rect.left, rect.top,
                rect.right - rect.left,
                rect.bottom - rect.top,
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);

        }

        void Win32Window::Move(const int16_t delta_x, const int16_t delta_y)
        {
            RECT rect;
            GetWindowRect(fHandleWindow, &rect);
            MoveWindow(fHandleWindow, rect.left + delta_x, rect.top + delta_y, rect.right - rect.left, rect.bottom - rect.top, false);
        }

        void Win32Window::ToggleFullScreen(bool multiMonitor)
        {
            HWND hwnd = fHandleWindow;

            switch (fFullSceenState)
            {
            case FullSceenState::Windowed:
                SetFullScreen(multiMonitor);
                break;
            case FullSceenState::SingleScreen:
                if (multiMonitor == true)
                    SetFullScreen(multiMonitor);
                else
                    SetWindowed();
                break;
            case FullSceenState::MultiScreen:
                SetWindowed();
                break;
            }


        }

        void Win32Window::DestroyResources()
        {
            if (fDragAndDrop != nullptr)
                fDragAndDrop->Detach();
        }


        LRESULT Win32Window::GetCorner(const POINTS& points) const
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
                LL_EXCEPTION_UNEXPECTED_VALUE;
            }

            return corner;
        }

        void Win32Window::SetVisible(bool visible)
        {
            if (GetVisible() != visible)
            {
                fVisible = visible;
                ShowWindow(fHandleWindow, visible == true ? SW_SHOW : SW_HIDE);
            }
        }

        LLUtils::PointI32 Win32Window::GetWindowSize() const
        {
            RECT r;
            GetWindowRect(fHandleWindow, &r);
            return{ r.right - r.left, r.bottom - r.top };
        }

        LRESULT Win32Window::WindowProc(const WinMessage& message)
        {
            LRESULT retValue = 0;
            bool defaultProc = true;
            switch (message.message)
            {
            case WM_NCHITTEST:

                if (GetTransparent() == true)
                    return HTTRANSPARENT;

                if (DefWindowProc(message.hWnd, message.message, message.wParam, message.lParam) == HTCLIENT)
                {
                    switch (fLockMouseToWindowMode)
                    {
                    case LockMouseToWindowMode::NoLock:
                        //Do nothing
                        break;
                    case LockMouseToWindowMode::LockResize:
                        defaultProc = false;
                        retValue = GetCorner(*(POINTS*)&message.lParam);
                        break;
                    case LockMouseToWindowMode::LockMove:
                    {
                        retValue = HTCAPTION;
                        defaultProc = false;
                    }
                    break;
                    default:
                        LL_EXCEPTION_UNEXPECTED_VALUE;
                    }
                }
                break;
            case WM_SETCURSOR:
                if (GetMouseCursor() != nullptr)
                {
                    ::SetCursor(fMouseCursor);
                    retValue = 1;
                    defaultProc = false;
                }
                break;
            case WM_MENUCHAR:
                if (GetEnableMenuChar() == false)
                {
                    defaultProc = false;
                    retValue = MAKELONG(0, MNC_CLOSE);
                }
                break;

            case WM_NCLBUTTONDBLCLK:

                switch (GetDoubleClickMode())
                {
                case DoubleClickMode::NotSet:
                    //Do nothing
                    break;
                case DoubleClickMode::Default:
                    if (DefWindowProc(message.hWnd, WM_NCHITTEST, message.wParam, message.lParam) != message.wParam)
                        defaultProc = false;
                    break;
                case DoubleClickMode::NonClientArea:
                case DoubleClickMode::ClientArea:
                case DoubleClickMode::EntireWindow:
                default:
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::NotImplemented, "");
                    break;
                }
                break;

            case WM_ERASEBKGND:
                defaultProc = GetEraseBackground();
                break;
            case WM_DESTROY:
                DestroyResources();
                break;

            case WM_SYSCOMMAND:
                switch (message.wParam)
                {
                case SC_MAXIMIZE:
                    fIsMaximized = true;
                    break;
                case SC_RESTORE:
                    fIsMaximized = false;
                    break;
                }
                break;
            case WM_CLOSE:
                NotifyRemovedForRelatedWindows();
                break;
            }

            EventWinMessage winEvent;
            winEvent.window = this;
            winEvent.message = message;
            RaiseEvent(winEvent);

            return (defaultProc == false ? retValue : DefWindowProc(message.hWnd, message.message, message.wParam, message.lParam));
        }



        LRESULT Win32Window::WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
        {
            //These are the message that been sent up to WM_CREATE
            // they won't be customly handle priot to WM_CREATE.
            //WM_GETMINMAXINFO                0x0024
            //WM_NCCREATE                     0x0081
            //WM_NCCALCSIZE                   0x0083
            //WM_CREATE                       0x0001
            if (message == WM_CREATE)
            {
                CREATESTRUCT* s = (CREATESTRUCT*)lParam;
                if (SetProp(hWnd, _T("windowClass"), s->lpCreateParams) == 0)
                    std::exception("Unable to set window property");
                reinterpret_cast<Win32Window*>(s->lpCreateParams)->fHandleWindow = hWnd;

            }

            Win32Window* window = reinterpret_cast<Win32Window*>(GetProp(hWnd, _T("windowClass")));
            if (window != nullptr)
                return window->WindowProc({ hWnd,message,wParam,lParam });
            else return DefWindowProc(hWnd, message, wParam, lParam);
        }

        POINT Win32Window::GetMousePosition() const
        {
            return Win32Helper::GetMouseCursorPosition(GetHandle());

        }


        SIZE Win32Window::GetClientSize() const
        {
            return Win32Helper::GetRectSize(GetClientRectangle());
        }

        RECT Win32Window::GetClientRectangle() const
        {
            RECT rect;
            GetClientRect(GetHandle(), &rect);
            return rect;
        }

        bool Win32Window::RaiseEvent(const Event& evnt)
        {
            bool handled = false;
            for (auto callback : fListeners)
                handled |= callback(&evnt);

            return handled;
        }

        void Win32Window::SetFocused() const
        {
            ::SetFocus(fHandleWindow);
        }
    }
}
