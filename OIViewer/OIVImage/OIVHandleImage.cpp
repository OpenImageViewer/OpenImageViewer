#include "OIVHandleImage.h"
namespace OIV
{
    OIVHandleImage::OIVHandleImage(ImageHandle handle, bool freeAtDescturction) : fFreeAtDescturction(freeAtDescturction)
    {
        SetImageHandle(handle);
        QueryImageInfo();
        GetDescriptorMutable().LoadTime = 0;
        GetDescriptorMutable().Source = ImageSource::GeneratedByLib;
    }
}