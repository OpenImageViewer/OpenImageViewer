#pragma once
#include <interfaces/IRenderer.h>

namespace OIV
{
    class GLRendererFactory
    {
    public:
        static IRendererSharedPtr  Create();
    };


}

