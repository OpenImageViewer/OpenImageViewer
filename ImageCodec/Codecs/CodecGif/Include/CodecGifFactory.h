#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecGifFactory
    {
    public:
        static IImagePlugin* Create();
    };
}

