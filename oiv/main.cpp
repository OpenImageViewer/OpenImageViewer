#include "precompiled.h"
#include "oiv.h"
#include  <iostream>
#include <memory>
#include "StringUtility.h"
#include "Api/functions.h"


#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#include <shellapi.h>
#endif


void HandleMessages(const MSG& uMsg)
{
    switch (uMsg.message)
    {
    case WM_KEYDOWN:
        if (uMsg.wParam == VK_ESCAPE)
            PostQuitMessage(0);
        break;
    case WM_MOUSEWHEEL:
        int zDelta = GET_WHEEL_DELTA_WPARAM(uMsg.wParam) / WHEEL_DELTA;
        
        CmdDataZoom zoom;
        //20% zoom in each step
        zoom.amount = zDelta * 0.20 ;
        if (OIV_Execute(CommandExecute::CE_Zoom, sizeof(CmdDataZoom), &zoom) != ResultCode::RC_Success)
            throw std::exception("Unable to Zoom.");
        break;
    }
}


int mainFunction(int argc, const wchar_t** argv)
{
    if (argc > 1)
    {


        CmdDataInit init;
        init.parentHandle = NULL;

        if (OIV_Execute(CommandExecute::CE_Init,sizeof(CmdDataInit), &init) != ResultCode::RC_Success)
            throw std::exception("Unable to initialize Image rendering engine.");


        //tstring filePath;
        std::wstring filePath;
        
        for (int i = 1; i < argc ; i++)
            filePath += argv[i];

        CmdDataLoadFile loadFile;
        //L"d:/PNG_transparency_demonstration_1.png";
        loadFile.filePath = const_cast<OIVCHAR*>(filePath.c_str());
        //loadFile.filePath = const_cast<OIVCHAR*>(<const OIVCHAR*>(filePath.c_str()));
        loadFile.FileNamelength = _tcslen(loadFile.filePath);
        if (OIV_Execute(CommandExecute::CE_LoadFile, sizeof(loadFile), &loadFile) != ResultCode::RC_Success)
            throw std::exception("Unable to Load image.");

        //MSG uMsg;
        //memset(&uMsg, 0, sizeof(uMsg));
        //while (WM_QUIT != uMsg.message)
        //{
        //    while (PeekMessage(&uMsg, NULL, 0, 0, PM_REMOVE) > 0) //Or use an if statement
        //    {
        //      
        //        //if (uMsg == WM_MOUSEWHEEL)

        //        TranslateMessage(&uMsg);
        //        DispatchMessage(&uMsg);
        //    }
        //    //Here is were all the "animation that isn't used when the user does something" code will go.
        //}


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
    else
    {
        std::cout << "invalid texture name";
    }
    return 0;
}


#ifdef WIN32

int CALLBACK WinMain(
    _In_ HINSTANCE hInstance,
    _In_ HINSTANCE hPrevInstance,
    _In_ LPSTR     lpCmdLine,
    _In_ int       nCmdShow
)
{
    LPWSTR *szArglist;
    int nArgs;
    szArglist = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    return mainFunction(nArgs, (const wchar_t**) szArglist);
}
#endif


int main(int argc, const wchar_t** argv)
{
    mainFunction(argc, argv);
}
