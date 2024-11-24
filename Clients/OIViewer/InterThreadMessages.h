#pragma once
#include <cstdint>
namespace OIV
{
    enum class InterThreadMessages : uint16_t
    {
        FileChanged,
        AutoScroll,
        FirstFrameDisplayed,
        LoadFileExternally,
        CountColors
    };

    struct CountColorsData
    {
        void* image;
        int64_t colorCount;
    };
}  // namespace OIV