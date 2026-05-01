#include <OIVAppCore/SubImagePolicy.h>

#include <algorithm>

namespace OIV
{
    bool SubImagePolicy::IsVisible(IMCodec::ImageItemType groupType, uint32_t subImageCount)
    {
        return groupType != IMCodec::ImageItemType::AnimationFrame && subImageCount > 0;
    }

    bool SubImagePolicy::IncludeMainImage(IMCodec::ImageItemType itemType)
    {
        return itemType != IMCodec::ImageItemType::Container;
    }

    uint16_t SubImagePolicy::TotalDisplayedImages(uint32_t subImageCount, bool includeMainImage)
    {
        return static_cast<uint16_t>(subImageCount + (includeMainImage ? 1 : 0));
    }

    int32_t SubImagePolicy::ActualImageIndexFromDisplayIndex(int32_t displayIndex, bool includeMainImage)
    {
        if (displayIndex == 0 && includeMainImage)
            return MainImageIndex;

        return includeMainImage ? std::max(0, displayIndex - 1) : displayIndex;
    }

    int32_t SubImagePolicy::InitialSelectionIndex(bool includeMainImage,
                                                  uint64_t mainImagePixels,
                                                  const std::vector<uint64_t>& subImagePixels)
    {
        uint64_t largestPixels = includeMainImage ? mainImagePixels : 0;
        int32_t largestDisplayIndex = includeMainImage ? MainImageIndex : 0;

        for (size_t i = 0; i < subImagePixels.size(); ++i)
        {
            if (subImagePixels[i] > largestPixels)
            {
                largestPixels = subImagePixels[i];
                largestDisplayIndex = static_cast<int32_t>(i + (includeMainImage ? 1 : 0));
            }
        }

        return largestDisplayIndex;
    }
}  // namespace OIV
