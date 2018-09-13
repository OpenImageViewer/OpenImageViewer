#pragma once
#include <windows.h>
namespace OIV
{
    namespace Win32
    {
        class UserMessage
        {
        public:
            static const UINT PRIVATE_WN_AUTO_SCROLL = WM_USER + 1;
            static const UINT PRIVATE_WN_NOTIFY_LOADED = WM_USER + 2;
            static const UINT PRIVATE_WM_REFRESH_TIMER = WM_USER + 3;
        };

    }
}
