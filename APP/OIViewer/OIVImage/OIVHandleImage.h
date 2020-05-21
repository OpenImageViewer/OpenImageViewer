#pragma once
#include "OIVBaseImage.h"
namespace OIV
{
    class OIVHandleImage : public OIVBaseImage
    {
    public:
        OIVHandleImage(ImageHandle handle, bool freeAtDestruction = true);
    };

    using OIVHandleImageSharedPtr = std::shared_ptr<OIVHandleImage>;
}