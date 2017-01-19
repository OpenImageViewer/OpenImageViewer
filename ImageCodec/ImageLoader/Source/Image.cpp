#include <algorithm>
#include <chrono>
#include <thread>
#include "Image.h"
#include <stdexcept>


namespace IMCodec
{
    Image::Image(const ImageProperies& propeerties,const ImageData& imageData)
        : fProperies(propeerties), fImageData(imageData)

    {
    
    }
   
}
