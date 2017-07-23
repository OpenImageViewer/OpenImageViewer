#pragma once
#include "../../oiv/Interfaces/IRenderer.h"

namespace OIV
{
    class D3D11RendererFactory
    {
    public:
        static IRendererSharedPtr  Create();
    };


}

