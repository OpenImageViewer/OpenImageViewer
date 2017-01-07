#pragma once
//#include <windows.h>
#include <sstream>

namespace OIV
{
    class Logger
    {
    public:
        static void Log(const char* message)
        {
            std::stringstream ss;
            ss << message << "\n";
            //OutputDebugStringA(ss.str().c_str());
            
        }
    };
}
