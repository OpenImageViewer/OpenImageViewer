#pragma once
#include <windowsx.h>
namespace OIV
{
    class TestApp
    {
    public:
        void Run(std::wstring filePath);
        
        void HandleMessages(const MSG& uMsg);
    };
}