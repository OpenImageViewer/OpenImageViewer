#pragma once
#include <interfaces/IRenderer.h>

namespace OIV
{
    class D3D11RendererFactory
    {
    public:
        static IRendererSharedPtr  Create();
    };


}

