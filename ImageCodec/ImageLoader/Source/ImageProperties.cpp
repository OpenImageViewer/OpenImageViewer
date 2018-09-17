#include "ImageProperties.h"
#include <limits>

namespace IMCodec
{
    bool ImageDescriptor::IsInitialized() const
    {
        return true
            && fData.GetBuffer() != nullptr
            && fProperties.IsInitialized() == true;
    }
}
