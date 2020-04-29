#include "../Include/CodecPNGFactory.h"
#include "CodecPNG.h"

namespace IMCodec
{
    IImagePlugin* CodecPNGFactory::Create()
    {
        return new CodecPNG();
    }
}
