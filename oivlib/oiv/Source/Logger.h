#pragma once
#ifdef _WIN32
#include <windows.h>
#endif

namespace OIV
{
    class Logger
    {
    public:
        static void Log(std::string message)
        {
        #ifdef _WIN32
                OutputDebugStringA((message + "\n") .c_str());
        #endif
      
        }
    };
}
