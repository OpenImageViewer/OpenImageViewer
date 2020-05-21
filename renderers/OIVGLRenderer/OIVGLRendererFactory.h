#pragma once
#include "../oiv/Interfaces/IRenderer.h"

namespace OIV
{
    class GLRendererFactory
    {
    public:
        static IRendererSharedPtr  Create();
    };


}

