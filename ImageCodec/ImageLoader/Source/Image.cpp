#include <algorithm>
#include <chrono>
#include <thread>
#include "Image.h"
#include <stdexcept>


namespace IMCodec
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
