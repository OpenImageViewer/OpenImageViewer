#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecFreeImageFactory
    {
    public:
        static IImagePlugin* Create();
    };
}

