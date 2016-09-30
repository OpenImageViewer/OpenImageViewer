#include "PreCompiled.h"
#ifdef _MSC_VER 
#include <windows.h>
#include <shellapi.h>
#endif

#include "TestApp.h"


int mainFunction(int argc, const wchar_t** argv)
{
    if (argc > 1)
    {
        std::wstring filePath;
        filePath = argv[1];
        for (int i = 2; i < argc; i++)
            filePath += std::wstring(L" ") + argv[i];
        

        OIV::TestApp testApp;
        testApp.Run(filePath);
        return 0;
    }

    return 1;
       
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


int _tmain(int argc, const TCHAR** argv)
{
    mainFunction(argc, argv);
}
