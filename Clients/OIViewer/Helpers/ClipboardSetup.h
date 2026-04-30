#pragma once

#include <Windows.h>

namespace OIV
{
    class ClipboardSetup
    {
      public:
        template <typename Clipboard>
        static void RegisterDefaultFormats(Clipboard& clipboard)
        {
            clipboard.RegisterFormat(CF_DIBV5);
            clipboard.RegisterFormat(CF_DIB);
            clipboard.RegisterFormat(CF_UNICODETEXT);
            clipboard.RegisterFormat(CF_TEXT);
        }
    };
}  // namespace OIV
