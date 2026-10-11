#pragma once
#include <RendererCore/IRenderBackend.h>

namespace OIV
{
    class D3D11RendererFactory
    {
      public:

        static RenderBackendSharedPtr Create();
    };

}  // namespace OIV
