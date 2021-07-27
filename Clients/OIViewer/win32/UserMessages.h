#pragma once
#include <windows.h>
namespace OIV
{
    namespace Win32
    {
        class UserMessage
        {
        public:
            static const UINT PRIVATE_WN_AUTO_SCROLL            = WM_USER + 1;
            static const UINT PRIVATE_WN_NOTIFY_LOADED          = WM_USER + 2;
            static const UINT PRIVATE_WM_REFRESH_TIMER          = WM_USER + 3;
            static const UINT PRIVATE_WN_FIRST_FRAME_DISPLAYED  = WM_USER + 4;
            static const UINT PRIVATE_WN_NOTIFY_USER_MESSAGE    = WM_USER + 5;
            static const UINT PRIVATE_WM_NOTIFY_FILE_CHANGED    = WM_USER + 6;
            static const UINT PRIVATE_WM_LOAD_FILE_EXTERNALLY   = WM_USER + 7;
        };
    }
}
