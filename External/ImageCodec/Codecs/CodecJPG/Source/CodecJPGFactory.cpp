#include "../Include/CodecjpgFactory.h"
#include "CodecJpeg.h"

namespace IMCodec
{
    IImagePlugin* CodecJPGFactory::Create()
    {
        return new CodecJpeg();
    }
}
