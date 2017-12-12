#include "ImageProperties.h"
#include <limits>

namespace IMCodec
{
    bool ImageDescriptor::IsInitialized() const
    {
        return true
            && fData.GetConstBuffer() != nullptr
            && fProperties.IsInitialized() == true;
    }
}
