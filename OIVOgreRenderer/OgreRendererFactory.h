#pragma once
#include <interfaces/IRenderer.h>

namespace OIV
{
    class OgreRendererFactory
    {
    public:
        static IRendererSharedPtr  Create();
    };


}

