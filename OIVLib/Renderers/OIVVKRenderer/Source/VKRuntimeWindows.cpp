#include <windows.h>
#include "VKRuntime.h"
#include <stdexcept>
#include <string>

namespace OIV
{
#define OIV_VK_FUNCTION(name) PFN_##name name = nullptr;
#include "VKFunctions.inc"
#undef OIV_VK_FUNCTION

    namespace
    {
        std::string LoadVulkanRuntime()
        {
            // The isolated test compiles this same loader against an absent module.
#ifdef OIV_VK_TEST_MISSING_RUNTIME
            constexpr auto libraryName = L"oiv-test-missing-vulkan-runtime.dll";
#else
            constexpr auto libraryName = L"vulkan-1.dll";
#endif
            const HMODULE module = LoadLibraryExW(libraryName, nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
            if (!module)
                return "Vulkan runtime could not be loaded (Windows error " + std::to_string(GetLastError()) + ")";

            // Retain the module for process lifetime, including partial resolution failure.
            // Publish a complete set of device-independent exports before initialization can proceed.
            struct Dispatch
            {
#define OIV_VK_FUNCTION(name) PFN_##name name;
#include "VKFunctions.inc"
#undef OIV_VK_FUNCTION
            } resolved{};
#define OIV_VK_FUNCTION(name)                                                                                          \
    resolved.name = reinterpret_cast<PFN_##name>(GetProcAddress(module, #name));                                       \
    if (!resolved.name)                                                                                                \
        return "Vulkan runtime does not export " #name;
#include "VKFunctions.inc"
#undef OIV_VK_FUNCTION
#define OIV_VK_FUNCTION(name) name = resolved.name;
#include "VKFunctions.inc"
#undef OIV_VK_FUNCTION
            return {};
        }
    }  // namespace

    void EnsureVulkanRuntime()
    {
        // Thread-safe local initialization caches success or failure for all renderer instances.
        static const std::string failure = LoadVulkanRuntime();
        if (!failure.empty())
            throw std::runtime_error(failure);
    }
}  // namespace OIV
