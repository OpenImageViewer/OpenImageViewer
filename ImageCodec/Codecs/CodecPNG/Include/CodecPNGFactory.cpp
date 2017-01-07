#include "CodecPNGFactory.h"
#include "../Source/CodecPNG.h"

namespace IMCodec
{
    IImagePlugin* CodecPNGFactory::Create()
    {
        return new CodecPNG();
    }
}
