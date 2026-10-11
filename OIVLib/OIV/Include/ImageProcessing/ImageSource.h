#pragma once
#include <cstdint>

namespace OIV
{
    constexpr int64_t UniqueColorsUninitialized = -1;
    constexpr int64_t UniqueColorsFailed        = -2;
    // Describes how document pixels entered the application. It carries no renderer identity.
    enum class ImageSource
    {
        // No document origin has been selected.
        None,
        // Decoded file pixels with a normalized document path.
        File,
        // Raw clipboard image pixels without a file path.
        Clipboard,
        // Raw clipboard image pixels without a file path.
        // CPU-rasterized clipboard text without a file path.
        ClipboardText,
        // Internal overlay text, independent of the opened document.
        InternalText,
        // Generated or edited pixels without a file path.
        GeneratedByLib
    };

}  // namespace OIV
