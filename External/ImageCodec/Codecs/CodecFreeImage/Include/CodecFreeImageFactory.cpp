#include "../Include/CodecFreeImageFactory.h"
#include "CodecFreeImage.h"

namespace IMCodec
{
    IImagePlugin* CodecFreeImageFactory::Create()
    {
        return new CodecFreeImage();
    }
}
