#ifdef _MSC_VER 
#include <windows.h>
#include <shellapi.h>
#endif

#include "TestApp.h"
#include "./win32/UserMessages.h"


std::wstring CompileFilePathFromArguments(int argc, const wchar_t** argv)
{
    std::wstring filePath;

    if (argc > 1)
    {
        filePath = argv[1];
        for (int i = 2; i < argc; i++)
            filePath += std::wstring(L" ") + argv[i];
    }

    return filePath;
}

void RunApp(std::wstring filePath)
{
    OIV::TestApp testApp;
    testApp.Init(filePath);
    testApp.Run();
    testApp.Destroy();
}

int mainFunction(int argc, const wchar_t** argv)
{
    using namespace OIV;
    std::wstring filePath = CompileFilePathFromArguments(argc, argv);

    HWND window = nullptr;

    if (filePath.empty() == false)
    {
        HWND window = TestApp::FindTrayBarWindow();
        if (window != nullptr)
        {
            filePath = LLUtils::FileSystemHelper::ResolveFullPath(filePath);
            COPYDATASTRUCT copyData{};
            copyData.dwData = ::OIV::Win32::UserMessage::PRIVATE_WM_LOAD_FILE_EXTERNALLY;
            copyData.cbData = static_cast<DWORD>((filePath.length() + 1) * sizeof(decltype(filePath)::value_type));
            copyData.lpData = const_cast<wchar_t*>(filePath.c_str());
            ::SendMessage(window, WM_COPYDATA, ::OIV::Win32::UserMessage::PRIVATE_WM_LOAD_FILE_EXTERNALLY, (LPARAM)(LPVOID)&copyData);
        }
        else
        {
            RunApp(filePath);
        }
    }
    else if (window == nullptr)
    {
        RunApp(filePath);
    }
    
    return 0;
  }

#ifdef WIN32

int WinMain(
         [[maybe_unused]] HINSTANCE hInstance,
         [[maybe_unused]] HINSTANCE hPrevInstance,
         [[maybe_unused]] LPSTR     lpCmdLine,
         [[maybe_unused]] int       nCmdShow
)
{
    int nArgs;
    wchar_t** str = CommandLineToArgvW(GetCommandLineW(), &nArgs);
    return mainFunction(nArgs, const_cast<const wchar_t**>(str));
}
#endif


int _tmain(int argc, const TCHAR** argv)
{
    return mainFunction(argc, argv);
}
