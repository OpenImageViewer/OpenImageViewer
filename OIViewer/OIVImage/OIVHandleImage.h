#pragma once
#include "OIVBaseImage.h"
namespace OIV
{
    class OIVHandleImage : public OIVBaseImage
    {
    public:
        OIVHandleImage(ImageHandle handle, bool freeAtDescturction);

        ~OIVHandleImage()
        {
            if (fFreeAtDescturction == true)
                FreeImage();
        }

    private:
        const bool fFreeAtDescturction = false;
    };
}