#pragma once
#include <windows.h>

namespace OIV
{
    class Logger
    {
    public:
        static void Log(char* message)
        {
            OutputDebugStringA(message);
            OutputDebugStringA("\n");
        }
    };
}
