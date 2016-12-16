#include "OIVD3D11RendererFactory.h"
#include "D3D11Renderer.h"

namespace  OIV
{
    IRendererSharedPtr D3D11RendererFactory::Create()
    {
        return IRendererSharedPtr(new D3D11Renderer());
    }
}
