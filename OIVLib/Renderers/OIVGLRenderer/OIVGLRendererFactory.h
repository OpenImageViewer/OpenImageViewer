#pragma once
#include <Interfaces/IRenderer.h>

namespace OIV
{
    class GLRendererFactory
    {
      public:

        static IRendererSharedPtr Create();
    };

}  // namespace OIV
