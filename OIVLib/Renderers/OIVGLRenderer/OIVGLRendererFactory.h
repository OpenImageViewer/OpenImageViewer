#pragma once
#include "../OIV/Interfaces/IRenderer.h"

namespace OIV
{
    class GLRendererFactory
    {
    public:
        static IRendererSharedPtr  Create();
    };


}

