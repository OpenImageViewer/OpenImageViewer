#pragma once
#include <RendererCore/IRenderBackend.h>

namespace OIV
{
    std::unique_ptr<IRenderBackend> CreateD3D11Backend();
}
