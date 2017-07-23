#pragma once

#include <IImagePlugin.h>

namespace IMCodec
{
    class CodecTiffFactory
    {
    public:
        static IImagePlugin* Create();
    };
}

