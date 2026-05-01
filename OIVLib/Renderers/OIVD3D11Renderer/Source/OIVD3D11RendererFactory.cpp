#include "../Include/OIVD3D11RendererFactory.h"
#include "OIVD3D11Renderer.h"

namespace  OIV
{
    IRendererSharedPtr D3D11RendererFactory::Create()
    {
        return std::make_shared<OIVD3D11Renderer>();
    }
}
