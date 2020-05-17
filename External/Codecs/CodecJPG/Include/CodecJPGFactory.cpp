#include "../Include/CodecJPGFactory.h"
#include "CodecJpeg.h"

namespace IMCodec
{
    IImagePlugin* CodecJPGFactory::Create()
    {
        return new CodecJpeg();
    }
}
