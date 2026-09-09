#pragma once

#include "VKRuntime.h"
#include <stdexcept>
#include <string>

namespace OIV
{
    class VKException : public std::runtime_error
    {
      public:

        VKException(VkResult result, const std::string& message)
            : std::runtime_error(message + " (VkResult: " + std::to_string(result) + ")"), fResult(result)
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
