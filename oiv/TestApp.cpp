#include "PreCompiled.h"
#include "TestApp.h"
#include "Utility.h"
#include <limits>
#include <iomanip>
#include "win32/Win32Window.h"
#include <windowsx.h>

namespace OIV
{
    
    template <class T,class U>
    bool TestApp::ExecuteCommand(CommandExecute command, T* request, U* response)
    {
        return
            OIV_Execute(command, sizeof(T), request, sizeof(U), response) == ResultCode::RC_Success;
            
    }

    TestApp::TestApp() :
        fKeyboardPanSpeed(100)
        ,fKeyboardZoomSpeed(0.1)
        , fIsSlideShowActive(false)
        , fFilterlevel(0)
    {
        
        new MonitorInfo();
    }

    TestApp::~TestApp()
    {
        fCurrentListPosition = ListFilesIterator();
        delete MonitorInfo::getSingletonPtr();
    }

    HWND TestApp::GetWindowHandle()
    {
        return fWindow.GetHandle();
        using namespace Ogre;
        RenderWindow* rw = dynamic_cast<RenderWindow*>(Root::getSingleton().getRenderTarget("MainWindow"));
        HWND handle = NULL;
        rw->getCustomAttribute("WINDOW", &handle);
        return handle;
    }

    void TestApp::UpdateFileInfo(const CmdResponseLoad& loadResponse)
    {

        std::wstringstream ss;
        
        ss << loadResponse.width << L" X " << loadResponse.height << L" X " 
            << loadResponse.bpp << L" BPP | loaded in " << std::fixed << std::setprecision(3) << loadResponse.loadTime << " ms";
        fWindow.SetStatusBarText(ss.str(), 0, 0);
        //HWND handle = GetWindowHandle();

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
            
            ss << filePath << L" - OpenImageViewer";
            HWND handle = GetWindowHandle();
            SetWindowTextW(handle, ss.str().c_str());
            UpdateFileInddex();
            UpdateFileInfo(loadResponse);
            
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
        using namespace std::placeholders;
        HINSTANCE moduleHanle = GetModuleHandle(NULL);
        fWindow.Create(moduleHanle, SW_SHOW);
        fWindow.AddEventListener(std::bind(&TestApp::HandleMessages, this,_1));
        CmdDataInit init;
        init.parentHandle = reinterpret_cast<size_t>(fWindow.GetHandle());
         
        ExecuteCommand(CommandExecute::CE_Init, &init, &CmdNull());
        LoadFile(filePath, false);
        SetFilterLevel(1);
        LoadFileInFolder(filePath);
        UpdateFileInddex();


        
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
            }
        }


        std::cout << "Program ended.";
    }

    void TestApp::UpdateFileInddex()
    {
        if (fListFiles.empty())
            return;
        int pos = std::distance(fListFiles.begin(), fCurrentListPosition) + 1;

        std::wstringstream ss;
        ss << L"File " << pos << L"/" << fListFiles.size();

        fWindow.SetStatusBarText(ss.str(), 1, 0);
    }

    void TestApp::JumpFiles(int step)
    {
        if (fListFiles.empty())
            return;

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
        }

        while ((isLoaded = LoadFile(*currentPos, true)) == false);

        if (isLoaded)
            fCurrentListPosition = currentPos;


        UpdateFileInddex();

    }
    
    void TestApp::ToggleFullScreen()
    {
        HWND hwnd = GetWindowHandle();

     
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

    void TestApp::handleKeyInput(const Win32WIndow::Win32Event& evnt)
    {
        bool IsAlt = GetKeyState(VK_MENU) & (USHORT)0x8000;
        bool IsControl = GetKeyState(VK_CONTROL) & (USHORT)0x8000;
        bool IsShift = GetKeyState(VK_SHIFT) & (USHORT)0x8000;

        switch (evnt.message.wParam)
        {
        case VK_ESCAPE:
            PostQuitMessage(0);
            break;
        case 'F':
            evnt.window->ToggleFullScreen(IsAlt ? true : false);
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
        case VK_NUMPAD8:
            Pan(0, -fKeyboardPanSpeed);
            break;
        case VK_NUMPAD2:
            Pan(0, fKeyboardPanSpeed);
            break;
        case VK_NUMPAD4:
            Pan(-fKeyboardPanSpeed,0);
            break;
        case VK_NUMPAD6:
            Pan(fKeyboardPanSpeed, 0);
            break;
        case VK_ADD:
            Zoom(fKeyboardZoomSpeed);
            break;
        case VK_SUBTRACT:
            Zoom(-fKeyboardZoomSpeed);
            break;
        case VK_NUMPAD5:
            // TODO: center
            break;
        case VK_MULTIPLY:
            //TODO: center and reset zoom
            break;

        }
    }

    void TestApp::Pan(int horizontalPIxels, int verticalPixels )
    {
        CmdDataPan pan;
        pan.x = horizontalPIxels;
        pan.y = verticalPixels;
        ExecuteCommand(CommandExecute::CE_Pan, &pan, &(CmdNull()));
    }

    void TestApp::Zoom(double precentage)
    {
        CmdDataZoom zoom;
        zoom.amount = precentage;
        ExecuteCommand(CommandExecute::CE_Zoom, &zoom, &CmdNull());

    }

    bool TestApp::HandleMessages(const Win32WIndow::Win32Event& evnt)
    {
        const MSG& uMsg = evnt.message;
        static int lastX = -1;// = std::numeric_limits<int>::min();
        static int lastY = -1; //= std::numeric_limits<int>::min();
        switch (uMsg.message)
        {
        case WM_WINDOWPOSCHANGED:
            ExecuteCommand(CE_Refresh, &CmdNull(), &CmdNull());
            break;
            case WM_SIZE:
            ExecuteCommand(CE_Refresh, &CmdNull(), &CmdNull());
            break;
            
        case WM_TIMER:
            {
            if (uMsg.wParam == cTimerID)
                JumpFiles(1);
            }
            break;

        case WM_KEYDOWN:
            case WM_SYSKEYDOWN:
            handleKeyInput(evnt);
            break;

        case WM_MOUSEWHEEL:
        {
            
            int zDelta = (GET_WHEEL_DELTA_WPARAM(uMsg.wParam) / WHEEL_DELTA);
            //20% percent zoom in each wheel step
            Zoom(zDelta * 0.2);
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
            if (GetCapture() == nullptr)
                return false;
            int xPos = GET_X_LPARAM(uMsg.lParam);
            int yPos = GET_Y_LPARAM(uMsg.lParam);

            int deltax = xPos - lastX;
            int deltaY = yPos - lastY;

            bool isLeftDown = uMsg.wParam & MK_LBUTTON;

            if (isLeftDown)
            {
                if (lastX != -1)
                {
                    Pan(-deltax, -deltaY);
                }

                lastX = xPos;
                lastY = yPos;
            }
        }
        break;
        }
        return true;
    }
}
