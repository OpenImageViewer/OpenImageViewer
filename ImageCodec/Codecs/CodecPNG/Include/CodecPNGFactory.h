#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecPNGFactory
    {
    public:
        static IImagePlugin* Create();
    };
}

