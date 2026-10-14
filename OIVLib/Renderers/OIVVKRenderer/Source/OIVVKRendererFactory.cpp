#include <OIVVKRendererFactory.h>
#include "VKRenderer.h"

namespace OIV
{
    std::unique_ptr<IRenderBackend> CreateVulkanBackend()
    {
        return std::make_unique<VKRenderer>();
    }
}  // namespace OIV
