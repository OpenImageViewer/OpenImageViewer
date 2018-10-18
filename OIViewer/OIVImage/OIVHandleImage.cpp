#include "OIVHandleImage.h"
namespace OIV
{
    OIVHandleImage::OIVHandleImage(ImageHandle handle)
    {
        SetImageHandle(handle);
        QueryImageInfo();
        GetDescriptorMutable().LoadTime = 0;
        GetDescriptorMutable().Source = ImageSource::GeneratedByLib;
    }
}