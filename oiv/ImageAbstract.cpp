#include "ImageAbstract.h"
#include <chrono>
namespace OIV
{
    Image::Image(const ImageProperies& propeerties, double loadTime)
         
    {
        fProperies = propeerties;
        fLoadTime = loadTime;
    }

     double Image::GetLoadTime() const
    {
        return fLoadTime;
    }
}