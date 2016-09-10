#include "PreCompiled.h"
#include "TestApp.h"
namespace OIV
{
    template <class T,class U>
    void TestApp::ExecuteCommand(CommandExecute command, T* request, U* response)
    {
        if (OIV_Execute(command, sizeof(T), request, sizeof(U), response)  != ResultCode::RC_Success)
            throw std::exception("Api function failed");
    }

    void TestApp::Run(std::wstring filePath)
    {
        CmdDataInit init;
        init.parentHandle = NULL;

        ExecuteCommand(CommandExecute::CE_Init, &init, &CmdNull());

        CmdDataLoadFile loadFile;
        loadFile.filePath = const_cast<OIVCHAR*>(filePath.c_str());
        loadFile.FileNamelength = _tcslen(loadFile.filePath);

        ExecuteCommand(CommandExecute::CE_LoadFile, &loadFile, &CmdNull());

        QryFileInformation fileInfo;
        ExecuteCommand(CE_GetFileInformation, &CmdNull(), &fileInfo);


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

    void TestApp::HandleMessages(const MSG & uMsg)
    {
        static int lastX = -1;// = std::numeric_limits<int>::min();
        static int lastY = -1; //= std::numeric_limits<int>::min();
        switch (uMsg.message)
        {
        case WM_KEYDOWN:
            if (uMsg.wParam == VK_ESCAPE)
                PostQuitMessage(0);
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