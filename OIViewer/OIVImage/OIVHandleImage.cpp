#include "OIVHandleImage.h"
namespace OIV
{
    OIVHandleImage::OIVHandleImage(ImageHandle handle, bool freeAtDestruction) : OIVBaseImage(freeAtDestruction)
    {
        SetImageHandle(handle);
        QueryImageInfo();
        GetDescriptorMutable().LoadTime = 0;
        GetDescriptorMutable().Source = ImageSource::GeneratedByLib;
    }
}