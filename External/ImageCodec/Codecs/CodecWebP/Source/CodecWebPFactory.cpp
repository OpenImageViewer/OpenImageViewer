#include "../Include/CodecWebPFactory.h"
#include "CodecWebP.h"

namespace IMCodec
{
    IImagePlugin* CodecWebPFactory::Create()
    {
        return new CodecWebP();
    }
}
