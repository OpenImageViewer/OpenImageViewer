#pragma once
#include <RendererCore/IRenderBackend.h>

namespace OIV
{
    class VKRendererFactory
    {
      public:

        static RenderBackendSharedPtr Create();
    };
}  // namespace OIV
