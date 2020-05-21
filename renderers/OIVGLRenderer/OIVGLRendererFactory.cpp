#include "OIVGLRendererFactory.h"
#include "OIVGLRenderer.h"

namespace  OIV
{
    IRendererSharedPtr GLRendererFactory::Create()
    {
        return IRendererSharedPtr(new OIVGLRenderer());
    }
}
