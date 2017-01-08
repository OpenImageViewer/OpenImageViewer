#include "ImageProperties.h"
#include <limits>

namespace IMCodec
{
    ImageProperies::ImageProperies()
    {
        ImageBuffer = nullptr;
        Type = TF_UNKNOWN;
        Width = std::numeric_limits<std::size_t>::max();
        Height = std::numeric_limits<std::size_t>::max();
        RowPitchInBytes = std::numeric_limits<std::size_t>::max();
        NumSubImages = std::numeric_limits<std::size_t>::max();
    }

    bool ImageProperies::IsInitialized() const
    {
        return true
            && ImageBuffer != nullptr
            && Type != TF_UNKNOWN
            && Width != std::numeric_limits<std::size_t>::max()
            && Height != std::numeric_limits<std::size_t>::max()
            && RowPitchInBytes != std::numeric_limits<std::size_t>::max()
            && NumSubImages != std::numeric_limits<std::size_t>::max();
    }
}
