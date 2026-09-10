#include "Helpers/ClipboardSetup.h"

#include <Windows.h>

namespace OIV
{
    void ClipboardSetup::RegisterDefaultFormats(LWS::Clipboard& clipboard)
    {
        // Prefer image data to text. Windows synthesizes legacy text formats from CF_UNICODETEXT.
        clipboard.RegisterFormat(CF_DIBV5);
        clipboard.RegisterFormat(CF_DIB);
        clipboard.RegisterFormat(CF_UNICODETEXT);
    }
}  // namespace OIV
