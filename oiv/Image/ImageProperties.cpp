#include "ImageProperties.h"
#include <limits>

namespace  OIV
{
    ImageProperies::ImageProperies()
    {
        ImageBuffer = nullptr;
        Type = IT_UNKNOWN;
        Width = std::numeric_limits<std::size_t>::max();
        Height = std::numeric_limits<std::size_t>::max();
        RowPitchInBytes = std::numeric_limits<std::size_t>::max();
        BitsPerTexel = std::numeric_limits<std::size_t>::max();
        NumSubImages = std::numeric_limits<std::size_t>::max();
    }

    bool ImageProperies::IsInitialized() const
    {
        return true
            && ImageBuffer != nullptr
            && Type != IT_UNKNOWN
            && Width != std::numeric_limits<std::size_t>::max()
            && Height != std::numeric_limits<std::size_t>::max()
            && RowPitchInBytes != std::numeric_limits<std::size_t>::max()
            && BitsPerTexel != std::numeric_limits<std::size_t>::max()
            && NumSubImages != std::numeric_limits<std::size_t>::max();
    }
}
