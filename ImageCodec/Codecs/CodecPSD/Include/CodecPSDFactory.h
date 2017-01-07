#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecPSDFactory
    {
    public:
        static IImagePlugin*  Create();
    };
}

