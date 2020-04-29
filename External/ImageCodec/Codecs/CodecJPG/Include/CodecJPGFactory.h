#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecJPGFactory
    {
    public:
        static IImagePlugin*  Create();
    };
}

