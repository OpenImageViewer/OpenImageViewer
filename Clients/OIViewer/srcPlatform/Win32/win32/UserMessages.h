#pragma once
#include <windows.h>
namespace OIV
{
    namespace Win32
    {
        class UserMessage
        {
          public:

            static constexpr UINT PRIVATE_WN_AUTO_SCROLL = WM_USER + 1;
            static constexpr UINT PRIVATE_WN_FIRST_FRAME_DISPLAYED = WM_USER + 2;
            static constexpr UINT PRIVATE_WM_NOTIFY_FILE_CHANGED = WM_USER + 3;
            static constexpr UINT PRIVATE_WM_LOAD_FILE_EXTERNALLY = WM_USER + 4;
        };
    }
}