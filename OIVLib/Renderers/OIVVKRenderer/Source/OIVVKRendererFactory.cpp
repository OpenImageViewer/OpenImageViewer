#include "OIVVKRendererFactory.h"
#include "VKRenderer.h"

namespace OIV
{
    IRendererSharedPtr VKRendererFactory::Create()
    {
        return std::make_shared<VKRenderer>();
    }
}  // namespace OIV
