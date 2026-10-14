#pragma once

#include "VKRuntime.h"
#include <RendererCore/RendererException.h>
#include <string>

namespace OIV
{
    inline RendererErrorCode ClassifyVkResult(VkResult result) noexcept
    {
        RendererErrorCode code = RendererErrorCode::BackendFailure;
        if (result == VK_ERROR_DEVICE_LOST)
            code = RendererErrorCode::DeviceLost;
        else if (result == VK_ERROR_OUT_OF_HOST_MEMORY || result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
            code = RendererErrorCode::OutOfMemory;
        else if (result == VK_ERROR_SURFACE_LOST_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
            code = RendererErrorCode::SurfaceUnavailable;
        return code;
    }

    class VKException : public RendererException
    {
      public:

        VKException(VkResult result, const std::string& operation)
            : RendererException(ClassifyVkResult(result), RendererType::Vulkan, operation,
                                RendererNativeError{RendererNativeDomain::Vulkan, static_cast<std::int64_t>(result)}),
              fResult(result)
        {
        }

        VkResult GetResult() const { return fResult; }

      private:

        VkResult fResult;
    };

    inline void CheckVkResult(VkResult result, const char* message = "Vulkan operation failed")
    {
        if (result != VK_SUCCESS)
            throw VKException(result, message);
    }
}  // namespace OIV
