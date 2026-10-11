#include "OIVGLRendererFactory.h"
#include "OIVGLRenderer.h"

namespace OIV
{
    RenderBackendSharedPtr GLRendererFactory::Create()
    {
        return std::make_shared<OIVGLRenderer>();
    }
}  // namespace OIV
