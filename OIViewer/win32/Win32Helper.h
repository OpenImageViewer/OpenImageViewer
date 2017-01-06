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

        static void MessageLoop()
        {
            MSG msg;
            BOOL bRet;
            while ((bRet = GetMessage(&msg, nullptr, 0, 0)) != 0)
            {
                if (bRet == -1)
                {
                    // handle the error and possibly exit
                }
                else
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }

        static POINT GetMouseCursorPosition()
        {
            POINT mousePos;
            GetCursorPos(&mousePos);
            return mousePos;
        }

        static void MoveMouse(PointI32 point)
        {

            POINT mousePos;
            ::GetCursorPos(&mousePos);
            ::SetCursorPos(mousePos.x + point.x, mousePos.y + point.y);
        }
    };
}