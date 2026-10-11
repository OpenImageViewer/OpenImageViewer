#pragma once
#include <RendererCore/IRenderBackend.h>

namespace OIV
{
    class GLRendererFactory
    {
      public:

        static RenderBackendSharedPtr Create();
    };

}  // namespace OIV
