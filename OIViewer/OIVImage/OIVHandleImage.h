#pragma once
#include "OIVBaseImage.h"
namespace OIV
{
    class OIVHandleImage : public OIVBaseImage
    {
    public:
        OIVHandleImage(ImageHandle handle);
    };

    using OIVHandleImageSharedPtr = std::shared_ptr<OIVHandleImage>;
}