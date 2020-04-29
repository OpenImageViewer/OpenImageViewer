#include "MainWindow.h"
#include <Buffer.h>
#include "BitmapHelper.h"
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
                    const std::wstring CursorsPath = LLUtils::StringUtility::ToNativeString(LLUtils::PlatformUtility::GetExeFolder()) + L"./Resources/Cursors/";

                    fCursors[0] = nullptr;
                    fCursors[(size_t)CursorType::SystemDefault] = LoadCursor(nullptr, IDC_ARROW);
                    fCursors[(size_t)CursorType::East] = LoadCursorFromFile((CursorsPath + L"arrow-E.cur").c_str());
                    fCursors[(size_t)CursorType::NorthEast] = LoadCursorFromFile((CursorsPath + L"arrow-NE.cur").c_str());
                    fCursors[(size_t)CursorType::North] = LoadCursorFromFile((CursorsPath + L"arrow-N.cur").c_str());
                    fCursors[(size_t)CursorType::NorthWest] = LoadCursorFromFile((CursorsPath + L"arrow-NW.cur").c_str());
                    fCursors[(size_t)CursorType::West] = LoadCursorFromFile((CursorsPath + L"arrow-W.cur").c_str());
                    fCursors[(size_t)CursorType::SouthWest] = LoadCursorFromFile((CursorsPath + L"arrow-SW.cur").c_str());
                    fCursors[(size_t)CursorType::South] = LoadCursorFromFile((CursorsPath + L"arrow-S.cur").c_str());
                    fCursors[(size_t)CursorType::SouthEast] = LoadCursorFromFile((CursorsPath + L"arrow-SE.cur").c_str());
                    fCursors[(size_t)CursorType::SizeAll] = LoadCursorFromFile((CursorsPath + L"arrow-C.cur").c_str());


                //    BitmapShaderPtr bitmap = Bitmap::FromFileAnyFormat((CursorsPath + L"arrow-C.cur").c_str());

                //struct {
                //    WORD             xHot;         // x hotspot
                //    WORD             yHot;         // y hotspot
                //    BITMAPINFOHEADER bih;
                //   } CURSOR_RES_HDR;

                //   

                // //Fill out pImage

                //
                //CURSOR_RES_HDR.xHot = 20;
                //CURSOR_RES_HDR.yHot = 20;
                //CURSOR_RES_HDR.bih = bitmap->GetBitmapHeader();
                //    
                //    

                //HCURSOR hcur = CreateIconFromResourceEx((BYTE*)&CURSOR_RES_HDR,
                //    bitmap->GetBitmapHeader().biSizeImage + 32, // size of image data + hotspot (in bytes)
                //    FALSE,
                //    0x00030000, // version: value mandated by windows
                //    0, 0,       // width & height, 0 means use default
                //    LR_DEFAULTSIZE | LR_DEFAULTCOLOR);


                ////LoadCursorFromFile();


                //fCursors[(size_t)CursorType::SizeAll] = hcur; 
                    fCursorsInitialized = true;
                }
                fCurrentCursorType = type;
                SetMouseCursor(fCurrentCursorType == CursorType::SystemDefault ? nullptr : fCursors[static_cast<int>(fCurrentCursorType)]);
            }
        }

        void MainWindow::OnCreate()
        {
            fHandleStatusBar = DoCreateStatusBar(GetHandle(), 12, GetModuleHandle(nullptr), 3);
            ResizeStatusBar();

            fCanvasWindow.Create();
            fCanvasWindow.SetParent(this);
            fCanvasWindow.SetVisible(true);
            fCanvasWindow.SetTransparent(true);

            SetStatusBarText(L"pixel: ", 0, SBT_NOBORDERS);
            SetStatusBarText(L"File: ", 1, 0);

            fImageControl.Create();
            fImageControl.SetParent(this);

            RawInput::ResiterWindow(GetHandle());
        }


#pragma region RawInput


        void MainWindow::FlushInput(bool calledFromIdleTimer)
        {

            uint64_t currentTime = static_cast<uint64_t>(fRawInputTimer.GetElapsedTimeInteger(LLUtils::StopWatch::TimeUnit::Milliseconds));

            if (currentTime - fRawInputLastEventDisptchTime >= fRawInputInterval)
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

        void MainWindow::HandleRawInput(RAWINPUT* event_raw_input)
        {

            switch (event_raw_input->header.dwType)
            {
            case RIM_TYPEMOUSE:
                HandleRawInputMouse(event_raw_input->data.mouse);
                break;
            case RIM_TYPEKEYBOARD:
                HandleRawInputKeyboard(event_raw_input->data.keyboard);
                break;
            default:
                LL_EXCEPTION_UNEXPECTED_VALUE;

            }
            FlushInput(false);
        }

        void MainWindow::SetInputFlushTimer(bool enable)
        {
            if (fInputFlushTimerEnabled != enable)
            {
                fInputFlushTimerEnabled = enable;

                if (fInputFlushTimerEnabled)
                    SetTimer(GetHandle(), cTimerIDRawInputFlush, 5, nullptr);
                else
                    KillTimer(GetHandle(), cTimerIDRawInputFlush);
            }
        }

        void MainWindow::HandleRawInputMouse(const RAWMOUSE& mouse)
        {
            fMouseState.Update(mouse);
        }

        void MainWindow::HandleRawInputKeyboard(const RAWKEYBOARD& keyboard)
        {
            //TODO: add support here for keyboard raw input.
        }


#pragma endregion



        HWND MainWindow::DoCreateStatusBar(HWND hwndParent, uint32_t idStatus, HINSTANCE hinst, uint32_t cParts)
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


            if (GetShowStatusBar() && GetFullScreenState() == FullSceenState::Windowed)
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
            fImageControl.SetVisible(isImageControlVisible);

            if (isImageControlVisible)
                SetWindowPos(fImageControl.GetHandle(), nullptr, clientSize.cx, 0, ImageListWidth, clientSize.cy, SWP_NOACTIVATE | SWP_NOZORDER);

            ShowWindow(fHandleStatusBar, GetFullScreenState() == FullSceenState::Windowed ? SW_SHOW : SW_HIDE);
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
            return Win32Helper::GetRectSize(rect);
        }



        LRESULT MainWindow::HandleWindwMessage(const Win32::Event* evnt1)
        {

            const EventWinMessage* evnt = dynamic_cast<const EventWinMessage*>(evnt1);
            if (evnt == nullptr)
                return 0;

            const WinMessage & message = evnt->message;

            LRESULT retValue = 0;
            bool defaultProc = true;
            switch (message.message)
            {
            case WM_CREATE:
                OnCreate();
                break;

            case WM_TIMER:
                if (message.wParam == cTimerIDRawInputFlush)
                    FlushInput(true);
                break;
            case WM_INPUT:
            {
                UINT dwSize;
                GetRawInputData((HRAWINPUT)message.lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));

                LLUtils::Buffer lpb(dwSize);

                if (lpb == nullptr)
                    return 0;

                if (GetRawInputData((HRAWINPUT)message.lParam,
                    RID_INPUT,
                    lpb.GetBuffer(),
                    &dwSize,
                    sizeof(RAWINPUTHEADER)) != dwSize)
                {
                    LL_EXCEPTION_SYSTEM_ERROR("can not get raw input data");
                }

                HandleRawInput(reinterpret_cast<RAWINPUT*>(lpb.GetBuffer()));
            }
            break;
            case WM_SIZE:
                HandleResize();
                break;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            }
            return retValue;
        }
    }
}