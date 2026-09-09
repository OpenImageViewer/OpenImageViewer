#pragma once
#include <Interfaces/IRenderer.h>

namespace OIV
{
    class VKRendererFactory
    {
      public:

        static IRendererSharedPtr Create();
    };
}  // namespace OIV
