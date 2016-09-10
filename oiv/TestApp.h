#pragma once
#include <windowsx.h>
namespace OIV
{
    class TestApp
    {
    public:
        void Run(std::wstring filePath);
        
        void HandleMessages(const MSG& uMsg);
        template<class T, class U>
        void ExecuteCommand(CommandExecute command, T * request, U * response);
    };
}