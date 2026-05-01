#pragma once

#include <ImageItem.h>

#include <cstdint>
#include <vector>

namespace OIV
{
    class SubImagePolicy
    {
      public:
        static constexpr int32_t MainImageIndex = -1;

        static bool IsVisible(IMCodec::ImageItemType groupType, uint32_t subImageCount);
        static bool IncludeMainImage(IMCodec::ImageItemType itemType);
        static uint16_t TotalDisplayedImages(uint32_t subImageCount, bool includeMainImage);
        static int32_t ActualImageIndexFromDisplayIndex(int32_t displayIndex, bool includeMainImage);
        static int32_t InitialSelectionIndex(bool includeMainImage,
                                             uint64_t mainImagePixels,
                                             const std::vector<uint64_t>& subImagePixels);
    };
}  // namespace OIV
