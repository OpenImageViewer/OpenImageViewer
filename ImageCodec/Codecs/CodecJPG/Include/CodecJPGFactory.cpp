#include "CodecjpgFactory.h"
#include "../Source/CodecJpeg.h"

namespace IMCodec
{
    IImagePlugin* CodecJPGFactory::Create()
    {
        return new CodecJpeg();
    }
}
