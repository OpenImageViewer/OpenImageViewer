#pragma once

#include <LWS/Clipboard.hpp>

namespace OIV
{
    // Transitional shim until LWS owns the default platform format set.
    class ClipboardSetup
    {
      public:

        static void RegisterDefaultFormats(LWS::Clipboard& clipboard);
    };
}  // namespace OIV
