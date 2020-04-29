#include "CodecFreeImageFactory.h"
#include "../Source/CodecFreeImage.h"

namespace IMCodec
{
    IImagePlugin* CodecFreeImageFactory::Create()
    {
        return new CodecFreeImage();
    }
}
