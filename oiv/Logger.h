#pragma once
#include <windows.h>

namespace OIV
{
    class Logger
    {
    public:
        static void Log(const char* message)
        {
            OutputDebugStringA(message);
            OutputDebugStringA("\n");
        }
    };
}
