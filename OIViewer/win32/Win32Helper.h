#pragma once
#include <windows.h>
namespace OIV
{
    class Win32Helper
    {
    public:
        static bool IsKeyPressed(DWORD virtualKey)
        {
            return (GetKeyState(virtualKey) & static_cast<USHORT>(0x8000)) != 0;

        }

        static bool IsKeyToggled(DWORD virtualKey)
        {
            return (GetKeyState(virtualKey) & static_cast<USHORT>(0x0001)) != 0;

        }
    };
}