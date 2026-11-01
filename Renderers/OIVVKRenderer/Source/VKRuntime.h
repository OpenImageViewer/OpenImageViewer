#pragma once
#include <vulkan/vulkan.h>
#if defined(_WIN32) && !defined(OIV_VK_TEST_DOUBLES)
namespace OIV
{
    // Exported loader trampolines support independent instances/devices. Never replace these
    // with process-global pointers returned for a particular physical or logical device.
    #define OIV_VK_FUNCTION(name) extern PFN_##name name;
    #include "VKFunctions.inc"
    #undef OIV_VK_FUNCTION
    // Call for Vulkan discovery/initialization only; help and metadata must work without a runtime.
    // Load failures are cached for process lifetime; fallback belongs to startup selection.
    void EnsureVulkanRuntime();
}  // namespace OIV
#endif
