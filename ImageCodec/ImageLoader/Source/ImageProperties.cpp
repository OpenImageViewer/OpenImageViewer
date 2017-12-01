#include "ImageProperties.h"
#include <limits>

namespace IMCodec
{
    bool ImageProperies::IsInitialized() const
    {
        return true
            && ImageBuffer != nullptr
            && TexelFormatDecompressed != TexelFormat::UNKNOWN
            && Width != std::numeric_limits<std::size_t>::max()
            && Height != std::numeric_limits<std::size_t>::max()
            && RowPitchInBytes != std::numeric_limits<std::size_t>::max()
            && NumSubImages != std::numeric_limits<std::size_t>::max();
    }
}
