#include "../Include/OIVD3D11RendererFactory.h"
#include "D3D11/D3D11Renderer.h"

namespace OIV
{
    std::unique_ptr<IRenderBackend> CreateD3D11Backend()
    {
        return std::make_unique<D3D11Renderer>();
    }
}  // namespace OIV
