#include "OIVVKRendererFactory.h"
#include "VKRenderer.h"

namespace OIV
{
    RenderBackendSharedPtr VKRendererFactory::Create()
    {
        return std::make_shared<VKRenderer>();
    }
}  // namespace OIV
