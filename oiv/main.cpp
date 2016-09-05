#include "precompiled.h"
#include "oiv.h"
#include  <iostream>
#include <memory>
#include "StringUtil.h"
#ifdef WIN32
#include <windows.h>
#include <stdio.h>
#include <shellapi.h>
#endif



int mainFunction(int argc, const wchar_t** argv)
{
    if (argc > 1)
    {
        std::unique_ptr<OIV> viewer = std::unique_ptr< OIV>(new OIV());
        //std::wstring texturename = argv[1];

        std::string filePAth;
        for (int i = 1; i < argc ; i++)
            filePAth += StringUtility::ToAString(argv[i]);
        
        
        viewer->SetTextureName(filePAth.c_str());
        viewer->Start();
        std::cout << "Render Ended";
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
