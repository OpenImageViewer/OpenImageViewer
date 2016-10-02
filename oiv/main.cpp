#include "PreCompiled.h"
#ifdef _MSC_VER 
#include <windows.h>
#include <shellapi.h>
#endif

#include "TestApp.h"


int mainFunction(int argc, const wchar_t** argv)
{
    std::wstring filePath;
    
    if (argc > 1)
    {
        filePath = argv[1];
        for (int i = 2; i < argc; i++)
            filePath += std::wstring(L" ") + argv[i];
    }

    OIV::TestApp testApp;
    testApp.Run(filePath);
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
    int nArgs;
    wchar_t** str = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    return mainFunction(nArgs, const_cast<const wchar_t**>(str));
}
#endif


int _tmain(int argc, const TCHAR** argv)
{
    mainFunction(argc, argv);
}
