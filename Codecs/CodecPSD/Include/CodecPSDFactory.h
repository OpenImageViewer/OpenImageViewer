#pragma once

#include <Interfaces\IImagePlugin.h>

namespace OIV
{
    class CodecPSDFactory
    {
    public:
        static IImagePlugin*  Create();
    };
}

