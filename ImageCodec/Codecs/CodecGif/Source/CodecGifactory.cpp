#include "../Include/CodecGifFactory.h"
#include "CodecGif.h"

namespace IMCodec
{
    IImagePlugin* CodecGifFactory::Create()
    {
        return new CodecGif();
    }
}
