#include "PreCompiled.h"
#include "TestApp.h"
#include "Utility.h"
#include <limits>
#include <algorithm>
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
     
    }

    bool TestApp::LoadFile(std::wstring filePath)
    {
        CmdDataLoadFile loadFile;
        loadFile.filePath = const_cast<OIVCHAR*>(filePath.c_str());
        loadFile.FileNamelength = _tcslen(loadFile.filePath);
        bool success  = ExecuteCommand(CommandExecute::CE_LoadFile, &loadFile, &CmdNull()) == true;

        if (success)
        {
            using namespace Ogre;
            RenderWindow* rw =  dynamic_cast<RenderWindow*>(Root::getSingleton().getRenderTarget("MainWindow"));

            QryFileInformation fileInfo;
            ExecuteCommand(CE_GetFileInformation, &CmdNull(), &fileInfo);
            

            std::wstringstream ss;
            
            ss << filePath <<L" | " << fileInfo.width << L" X " << fileInfo.height << L" X " << fileInfo.bitsPerPixel << L" BPP";
            HWND handle = NULL;
            rw->getCustomAttribute("WINDOW", &handle);
            SetWindowTextW(handle, ss.str().c_str());
        }

        return success;
    }

    void TestApp::LoadFileInFolder(std::wstring filePath)
    {
        int idx = filePath.find_last_of(L"\\");
        if (idx > -1)
        {
            std::wstring  folder = filePath.substr(0, idx);

            Utility::find_files(folder, fListFiles);
            fCurrentListPosition = std::find(fListFiles.begin(), fListFiles.end(), filePath);
        }
        else
            fCurrentListPosition = fListFiles.end();

    }

    void TestApp::Run(std::wstring filePath)
    {
        using namespace std;
        CmdDataInit init;
        init.parentHandle = NULL;

        ExecuteCommand(CommandExecute::CE_Init, &init, &CmdNull());
        
        LoadFile(filePath);

        

        LoadFileInFolder(filePath);


        HWND hwndMain;
        HWND hwndDlgModeless = NULL;
        MSG msg;
        BOOL bRet;
        HACCEL haccel;
        while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
        {
            if (bRet == -1)
            {
                // handle the error and possibly exit
            }
            else
            {
                if (hwndDlgModeless == (HWND)NULL ||
                    !IsDialogMessage(hwndDlgModeless, &msg) &&
                    !TranslateAccelerator(hwndMain, haccel,
                        &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                    HandleMessages(msg);
                }
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

        while ((isLoaded = LoadFile(*currentPos)) == false);

        if (isLoaded)
            fCurrentListPosition = currentPos;
    }

    void TestApp::HandleMessages(const MSG & uMsg)
    {
        static int lastX = -1;// = std::numeric_limits<int>::min();
        static int lastY = -1; //= std::numeric_limits<int>::min();
        switch (uMsg.message)
        {
        case WM_KEYDOWN:
            switch (uMsg.wParam)
            {
            case VK_ESCAPE:
                PostQuitMessage(0);
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