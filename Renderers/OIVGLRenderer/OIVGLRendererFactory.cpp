#include "OIVGLRendererFactory.h"
#include "OIVGLRenderer.h"

namespace OIV
{
    std::unique_ptr<IRenderBackend> CreateOpenGLBackend()
    {
        return std::make_unique<OIVGLRenderer>();
    }
}  // namespace OIV
