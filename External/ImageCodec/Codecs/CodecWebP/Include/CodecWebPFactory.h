#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecWebPFactory
    {
    public:
        static IImagePlugin* Create();
    };
}

