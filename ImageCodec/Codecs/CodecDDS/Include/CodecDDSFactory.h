#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecDDSFactory
    {
    public:
        static IImagePlugin* Create();
    };
}

