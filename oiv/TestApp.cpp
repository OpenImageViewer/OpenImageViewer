#include "PreCompiled.h"
#include "TestApp.h"
#include "Utility.h"
#include <limits>
#include <iomanip>
namespace OIV
{
    
    template <class T,class U>
    bool TestApp::ExecuteCommand(CommandExecute command, T* request, U* response)
    {
        return
            OIV_Execute(command, sizeof(T), request, sizeof(U), response) == ResultCode::RC_Success;
            
    }

    TestApp::TestApp()
    {
        fLastWindowPlacement = { 0 };
        fIsSlideShowActive = false;
        fFilterlevel = 0;
    }

    HWND TestApp::GetWindowHandle()
    {
        using namespace Ogre;
        RenderWindow* rw = dynamic_cast<RenderWindow*>(Root::getSingleton().getRenderTarget("MainWindow"));
        HWND handle = NULL;
        rw->getCustomAttribute("WINDOW", &handle);
        return handle;
    }

    bool TestApp::LoadFile(std::wstring filePath,bool onlyRegisteredExtension)
    {
        CmdResponseLoad loadResponse;
        CmdDataLoadFile loadRequest;
        loadRequest.filePath = const_cast<OIVCHAR*>(filePath.c_str());
        loadRequest.FileNamelength = _tcslen(loadRequest.filePath);
        loadRequest.onlyRegisteredExtension = onlyRegisteredExtension;

        bool success  = ExecuteCommand(CommandExecute::CE_LoadFile, &loadRequest, &loadResponse) == true;

        if (success)
        {
            using namespace Ogre;
            /*QryFileInformation fileInfo;
            ExecuteCommand(CE_GetFileInformation, &CmdNull(), &fileInfo);*/

            std::wstringstream ss;
            
            ss << filePath <<L" | " << loadResponse.width << L" X " << loadResponse.height << L" X " 
                << loadResponse.bpp << L" BPP | loaded in " << std::fixed << std::setprecision(3) << loadResponse.loadTime << " ms";
            HWND handle = GetWindowHandle();
            
            SetWindowTextW(handle, ss.str().c_str());
        }

        return success;
    }

    void TestApp::LoadFileInFolder(std::wstring filePath)
    {
        
        std::wstring workingFolder;
        int idx = filePath.find_last_of(L"\\");

        if (idx > -1)
        {
            workingFolder = filePath.substr(0, idx);
        }
        else
        {
            TCHAR path[MAX_PATH];
            if (GetCurrentDirectory(MAX_PATH, reinterpret_cast<LPTSTR>(&path)) != 0)
            {
                workingFolder = path;
                filePath = workingFolder + L"\\" + filePath;
            }

        }

        if (workingFolder.empty() == false)
        {
            Utility::find_files(workingFolder, fListFiles);
            fCurrentListPosition = std::find(fListFiles.begin(), fListFiles.end(), filePath);
        }
        else
        {
            fCurrentListPosition = fListFiles.end();
        }

    }

    void TestApp::Run(std::wstring filePath)
    {
        using namespace std;
        CmdDataInit init;
        init.parentHandle = NULL;

        ExecuteCommand(CommandExecute::CE_Init, &init, &CmdNull());
        
        LoadFile(filePath, false);
        SetFilterLevel(1);

        

        LoadFileInFolder(filePath);


        
        MSG msg;
        BOOL bRet;
        while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
        {
            if (bRet == -1)
            {
                // handle the error and possibly exit
            }
            else
            {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                    HandleMessages(msg);
            }
        }


        std::cout << "Program ended.";
    }

    void TestApp::JumpFiles(int step)
    {
        if (fListFiles.empty())
            return;
        
        //std::list<tstring>::const_iterator
        ListFilesIterator currentPos = fCurrentListPosition;

        int sign;
        if (step == std::numeric_limits<int>::max())
        {
            
            currentPos = fListFiles.end(); // const_cast<ListFilesIterator&>(fListFiles.end());
            sign = -1;
        }
        else if (step == std::numeric_limits<int>::min())
        {
            currentPos = fListFiles.begin();
            sign = 1;
        }
        else
        {
            //currentPos = std::next(currentPos, step);
            sign = step > 0 ? 1 : -1;
        }

        bool isLoaded = false;
        do
        {
            if (currentPos == fListFiles.end() && sign == 1)
                break;
            if (currentPos == fListFiles.begin() && sign == -1)
                break;
            
            currentPos = std::next(currentPos, sign);
            
            if (currentPos == fListFiles.end() && sign == 1)
                break;
            if (currentPos == fListFiles.begin() && sign == -1)
                break;
        }

        while ((isLoaded = LoadFile(*currentPos, true)) == false);

        if (isLoaded)
            fCurrentListPosition = currentPos;
    }
    
    void TestApp::ToggleFullScreen()
    {
        HWND hwnd = GetWindowHandle();

        DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
        if (dwStyle & WS_OVERLAPPEDWINDOW) {
            MONITORINFO mi = { sizeof(mi) };
            if (GetWindowPlacement(hwnd, &fLastWindowPlacement) &&
                GetMonitorInfo(MonitorFromWindow(hwnd,
                    MONITOR_DEFAULTTOPRIMARY), &mi)) {
                SetWindowLong(hwnd, GWL_STYLE,
                    dwStyle & ~WS_OVERLAPPEDWINDOW);
                SetWindowPos(hwnd, HWND_TOP,
                    mi.rcMonitor.left, mi.rcMonitor.top,
                    mi.rcMonitor.right - mi.rcMonitor.left,
                    mi.rcMonitor.bottom - mi.rcMonitor.top,
                    SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
            }
        }
        else {
            SetWindowLong(hwnd, GWL_STYLE,
                dwStyle | WS_OVERLAPPEDWINDOW);
            SetWindowPlacement(hwnd, &fLastWindowPlacement);
            SetWindowPos(hwnd, NULL, 0, 0, 0, 0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    }

    void TestApp::ToggleBorders()
    {
        HWND hwnd = GetWindowHandle();

        DWORD dwStyle = GetWindowLong(hwnd, GWL_STYLE);
        if (dwStyle & WS_OVERLAPPEDWINDOW)
        {
            SetWindowLong(hwnd, GWL_STYLE,
                dwStyle & ~WS_OVERLAPPEDWINDOW);
        }
        else
        {
            SetWindowLong(hwnd, GWL_STYLE,
                dwStyle | WS_OVERLAPPEDWINDOW);

        }
    }

    void TestApp::ToggleSlideShow()
    {
        HWND hwnd = GetWindowHandle();
        
        if (fIsSlideShowActive == false)
            SetTimer(hwnd, cTimerID, 3000, NULL);
        else
            KillTimer(hwnd, cTimerID);
        fIsSlideShowActive = !fIsSlideShowActive;
        
    }

    void TestApp::SetFilterLevel(int filterLevel)
    {
        CmdRequestFilter filter;

        filter.filterLevel = filterLevel;
        if (ExecuteCommand(CE_FilterLevel, &filter, &CmdNull()))
            fFilterlevel = filterLevel;
    }

    void TestApp::HandleMessages(const MSG & uMsg)
    {
        static int lastX = -1;// = std::numeric_limits<int>::min();
        static int lastY = -1; //= std::numeric_limits<int>::min();
        switch (uMsg.message)
        {
        

        case WM_TIMER:
            {
            if (uMsg.wParam == cTimerID)
                JumpFiles(1);
            }
            break;

        case WM_KEYDOWN:
            switch (uMsg.wParam)
            {
            case VK_ESCAPE:
                PostQuitMessage(0);
                break;
            case 'F':
                ToggleFullScreen();
                break;
            case VK_DOWN:
            case VK_RIGHT:
            case VK_NEXT:
                JumpFiles(1);
                break;
            case VK_UP:
            case VK_LEFT:
            case VK_PRIOR:
                JumpFiles(-1);
                break;
            case VK_HOME:
                JumpFiles(std::numeric_limits<int>::min());
                break;
            case VK_END:
                JumpFiles(std::numeric_limits<int>::max());
                break;
            case VK_SPACE:
                ToggleSlideShow();
                break;
            case 'B':
                ToggleBorders();
                break;
            case VK_OEM_PERIOD:
                SetFilterLevel(fFilterlevel + 1);
                break;
            case VK_OEM_COMMA:
                SetFilterLevel(fFilterlevel - 1);
                break;

            }
                
            break;
        case WM_MOUSEWHEEL:
        {
            int zDelta = GET_WHEEL_DELTA_WPARAM(uMsg.wParam) / WHEEL_DELTA;

            CmdDataZoom zoom;
            //20% zoom in each step
            zoom.amount = zDelta * 0.20;
            ExecuteCommand(CommandExecute::CE_Zoom, &zoom, &CmdNull());
            
        }
        break;

        case WM_LBUTTONDOWN:
            SetCapture(uMsg.hwnd);
            break;

        case WM_LBUTTONUP:
        {
            ReleaseCapture();
            lastX = -1;
        }

        case WM_MOUSEMOVE:
        {
            if (GetCapture() == NULL)
                return;
            int xPos = GET_X_LPARAM(uMsg.lParam);
            int yPos = GET_Y_LPARAM(uMsg.lParam);

            int deltax = xPos - lastX;
            int deltaY = yPos - lastY;

            bool isLeftDown = uMsg.wParam & MK_LBUTTON;

            if (isLeftDown)
            {
                if (lastX != -1)
                {
                    CmdDataPan pan;
                    ////20% zoom in each step
                    pan.x = -deltax / 200.0;
                    pan.y = -deltaY / 200.0;
                    ExecuteCommand(CommandExecute::CE_Pan, &pan, &CmdNull());
                }

                lastX = xPos;
                lastY = yPos;
            }
        }




        break;
        }
    }
}