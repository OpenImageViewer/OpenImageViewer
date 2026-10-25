#pragma once

#include <Windows.h>

namespace IMCodec
{
    class ImageLoader;
}

namespace OIV::Win32
{
    // CPU-only preview for the WLX adapter. The caller owns the returned bitmap and releases it with DeleteObject.
    // Reuse a loader on its owning thread. Concurrent calls need separate loaders. Invalid input or decoding failure
    // returns nullptr. The first displayable image is fitted without enlargement onto a white bitmap of width x height.
    // The adapter should pass the full filename: WLX's optional content buffer may contain only a file prefix.
    [[nodiscard]] HBITMAP CreatePreviewBitmap(IMCodec::ImageLoader& loader, const wchar_t* fileName, int width,
                                              int height) noexcept;
}  // namespace OIV::Win32
