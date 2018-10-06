#pragma once
namespace OIV
{
    namespace Win32
    {
        struct WinMessage
        {
            HWND hWnd;
            UINT message;
            WPARAM wParam;
            LPARAM lParam;
        };

    }
}
