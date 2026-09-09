#include "VKContext.h"
#include "VKCommon.h"
#include <Interfaces/RendererOptions.h>

#include <algorithm>
#include <cstring>
#include <iostream>
#include <array>
#include <stdexcept>

namespace OIV
{
    VKContext::VKContext() = default;

    VKContext::~VKContext()
    {
        Purge();
    }

    void VKContext::Init(const CreateParams& params)
    {
        fGpuIndex = params.gpuIndex;
        fWindow   = params.window;
#ifdef __linux__
        fNativeDisplay = params.nativeDisplay;
#endif

        if (fWindow == 0)
            throw std::invalid_argument("Cannot create a Vulkan surface for an empty window handle");

        CreateInstance();
        CreateSurface(params.nativeDisplay, params.window);
        PickPhysicalDevice(params.adapterName);
        CreateLogicalDevice();
        CreateSwapChain(params.width, params.height);
        CreateCommandPool();
    }

    void VKContext::Purge()
    {
        if (fDevice != VK_NULL_HANDLE)
        {
            vkDeviceWaitIdle(fDevice);

            CleanupSwapChain();

            if (fCommandPool != VK_NULL_HANDLE)
            {
                vkDestroyCommandPool(fDevice, fCommandPool, nullptr);
                fCommandPool = VK_NULL_HANDLE;
            }

            vkDestroyDevice(fDevice, nullptr);
            fDevice = VK_NULL_HANDLE;
        }

        if (fSurface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(fInstance, fSurface, nullptr);
            fSurface = VK_NULL_HANDLE;
        }

        if (fInstance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(fInstance, nullptr);
            fInstance = VK_NULL_HANDLE;
        }
    }

    void VKContext::RecreateSurface()
    {
        if (fSurface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(fInstance, fSurface, nullptr);
            fSurface = VK_NULL_HANDLE;
        }
#ifdef __linux__
        CreateSurface(fNativeDisplay, fWindow);
#else
        CreateSurface(nullptr, fWindow);
#endif
    }

    bool VKContext::RecreateSwapChain(int width, int height, bool recoverSurface)
    {
        CleanupSwapChain();
        if (recoverSurface)
        {
            RecreateSurface();
            VkBool32 presentSupport{VK_FALSE};
            if (vkGetPhysicalDeviceSurfaceSupportKHR(fPhysicalDevice, fPresentQueueFamily, fSurface, &presentSupport) !=
                    VK_SUCCESS ||
                !presentSupport)
                throw std::runtime_error("The Vulkan present queue cannot use the recreated surface");
        }
        return CreateSwapChain(width, height);
    }

    void VKContext::CreateInstance()
    {
#if defined(_WIN32) && !defined(OIV_VK_TEST_DOUBLES)
        EnsureVulkanRuntime();
#endif
        VkApplicationInfo appInfo{};
        appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName   = "OIV";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName        = "OIV";
        appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion         = VK_API_VERSION_1_1;

        static constexpr const char* extensions[] = {
            VK_KHR_SURFACE_EXTENSION_NAME,
#ifdef _WIN32
            VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
            VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME,
#else
            VK_KHR_XLIB_SURFACE_EXTENSION_NAME,
#endif
        };

        VkInstanceCreateInfo createInfo{};
        createInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo        = &appInfo;
        createInfo.enabledExtensionCount   = static_cast<uint32_t>(std::size(extensions));
        createInfo.ppEnabledExtensionNames = extensions;

        VkInstance instance;
        VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
        CheckVkResult(result, "Failed to create Vulkan instance");
        fInstance = instance;
    }

    void VKContext::CreateSurface(void* nativeDisplay, std::uintptr_t window)
    {
        VkSurfaceKHR surface{VK_NULL_HANDLE};

#ifdef _WIN32
        const HWND nativeWindow  = reinterpret_cast<HWND>(window);
        HINSTANCE nativeInstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(nativeWindow, GWLP_HINSTANCE));
        if (nativeInstance == nullptr)
            nativeInstance = GetModuleHandle(nullptr);

        VkWin32SurfaceCreateInfoKHR createInfo{};
        createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        createInfo.hinstance = nativeInstance;
        createInfo.hwnd      = nativeWindow;

        VkResult result = vkCreateWin32SurfaceKHR(fInstance, &createInfo, nullptr, &surface);
        CheckVkResult(result, "Failed to create Win32 surface");
#elif defined(__linux__)
    #ifdef VK_USE_PLATFORM_WAYLAND_KHR
        VkWaylandSurfaceCreateInfoKHR createInfo{};
        createInfo.sType   = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
        createInfo.display = reinterpret_cast<wl_display*>(nativeDisplay);
        createInfo.surface = reinterpret_cast<wl_surface*>(window);

        VkResult result = vkCreateWaylandSurfaceKHR(fInstance, &createInfo, nullptr, &surface);
        CheckVkResult(result, "Failed to create Wayland surface");
    #else
        VkXlibSurfaceCreateInfoKHR createInfo{};
        createInfo.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
        createInfo.dpy    = reinterpret_cast<Display*>(nativeDisplay);
        createInfo.window = static_cast<Window>(window);

        VkResult result = vkCreateXlibSurfaceKHR(fInstance, &createInfo, nullptr, &surface);
        CheckVkResult(result, "Failed to create X11 surface");
    #endif
#endif

        fSurface = surface;
    }

    namespace
    {
        constexpr Acceleration ClassifyDevice(VkPhysicalDeviceType type)
        {
            switch (type)
            {
                case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                    return Acceleration::Hardware;
                case VK_PHYSICAL_DEVICE_TYPE_CPU:
                    return Acceleration::Software;
                default:
                    return Acceleration::Unknown;
            }
        }
    }  // namespace

    std::vector<RendererAdapter> VKContext::EnumerateAdapters()
    {
        VKContext probe;
        probe.CreateInstance();
        uint32_t count{};
        CheckVkResult(vkEnumeratePhysicalDevices(probe.fInstance, &count, nullptr),
                      "Failed to enumerate Vulkan devices");
        std::vector<VkPhysicalDevice> devices(count);
        if (count)
            CheckVkResult(vkEnumeratePhysicalDevices(probe.fInstance, &count, devices.data()),
                          "Failed to enumerate Vulkan devices");
        std::vector<RendererAdapter> adapters;
        adapters.reserve(count);
        for (uint32_t i = 0; i < count; ++i)
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(devices[i], &properties);
            adapters.push_back({static_cast<int>(i), properties.deviceName, properties.vendorID,
                                ClassifyDevice(properties.deviceType),
                                properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU});
        }
        return adapters;
    }

    Acceleration VKContext::GetAcceleration() const
    {
        return fPhysicalDevice == VK_NULL_HANDLE ? Acceleration::Unknown
                                                 : ClassifyDevice(GetPhysicalDeviceProperties().deviceType);
    }

    void VKContext::PickPhysicalDevice(const char* adapterName)
    {
        uint32_t deviceCount{0};
        CheckVkResult(vkEnumeratePhysicalDevices(fInstance, &deviceCount, nullptr),
                      "Failed to enumerate Vulkan physical devices");

        if (deviceCount == 0)
            throw std::runtime_error("Failed to find GPUs with Vulkan support");

        std::vector<VkPhysicalDevice> devices(deviceCount);
        CheckVkResult(vkEnumeratePhysicalDevices(fInstance, &deviceCount, devices.data()),
                      "Failed to enumerate Vulkan physical devices");

        const bool requested = fGpuIndex >= 0 || adapterName != nullptr;
        if (fGpuIndex >= static_cast<int>(devices.size()))
            throw std::invalid_argument("--adapter-index is outside the Vulkan adapter list");

        VkPhysicalDeviceProperties selectedProperties{};
        int selectedIndex  = -1;
        const size_t first = fGpuIndex >= 0 ? static_cast<size_t>(fGpuIndex) : 0;
        const size_t end   = fGpuIndex >= 0 ? first + 1 : devices.size();
        for (size_t i = first; i < end; ++i)
        {
            VkPhysicalDeviceProperties properties{};
            vkGetPhysicalDeviceProperties(devices[i], &properties);
            if (fGpuIndex < 0 && adapterName != nullptr &&
                !detail::AdapterNameMatches(adapterName, properties.deviceName, properties.vendorID))
                continue;
            // Descriptor pool growth relies on Vulkan 1.1's recoverable out-of-pool result.
            if (properties.apiVersion < VK_API_VERSION_1_1)
                continue;

            uint32_t graphicsQueueFamily{UINT32_MAX};
            uint32_t presentQueueFamily{UINT32_MAX};
            if (!IsPhysicalDeviceSuitable(devices[i], graphicsQueueFamily, presentQueueFamily))
                continue;

            const bool discrete = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
            if (selectedIndex < 0 || discrete)
            {
                fPhysicalDevice      = devices[i];
                fGraphicsQueueFamily = graphicsQueueFamily;
                fPresentQueueFamily  = presentQueueFamily;
                selectedIndex        = static_cast<int>(i);
                selectedProperties   = properties;
            }
            if (requested || discrete)
                break;
        }

        if (selectedIndex < 0)
            throw std::runtime_error(
                requested ? "The requested adapter was not found or does not support Vulkan 1.1 and window presentation"
                          : "Failed to find a Vulkan 1.1 GPU that can render and present to this window");

        fGpuIndex          = selectedIndex;
        const char* reason = requested ? "user requested"
                             : selectedProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                                 ? "discrete GPU preferred"
                                 : "first available GPU (no discrete GPU found)";
        std::cerr << "[VK] Selected GPU: " << selectedProperties.deviceName << " (index " << fGpuIndex << ", " << reason
                  << ")" << std::endl;
    }

    bool VKContext::IsPhysicalDeviceSuitable(VkPhysicalDevice device, uint32_t& graphicsQueueFamily,
                                             uint32_t& presentQueueFamily) const
    {
        uint32_t extensionCount{0};
        if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr) != VK_SUCCESS)
            return false;

        std::vector<VkExtensionProperties> extensions(extensionCount);
        if (vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, extensions.data()) != VK_SUCCESS)
            return false;

        const auto supportsExtension = [&](const char* requiredExtension)
        {
            return std::any_of(extensions.begin(), extensions.end(), [&](const VkExtensionProperties& extension)
                               { return std::strcmp(extension.extensionName, requiredExtension) == 0; });
        };

        if (!supportsExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME))
            return false;

        uint32_t queueFamilyCount{0};
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; ++i)
        {
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && graphicsQueueFamily == UINT32_MAX)
                graphicsQueueFamily = i;

            VkBool32 presentSupport{VK_FALSE};
            if (vkGetPhysicalDeviceSurfaceSupportKHR(device, i, fSurface, &presentSupport) != VK_SUCCESS)
                return false;
            if (presentSupport && presentQueueFamily == UINT32_MAX)
                presentQueueFamily = i;
            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 && presentSupport)
            {
                graphicsQueueFamily = i;
                presentQueueFamily  = i;
                break;
            }
        }

        uint32_t formatCount{0};
        uint32_t presentModeCount{0};
        const bool hasSwapChainSupport = vkGetPhysicalDeviceSurfaceFormatsKHR(device, fSurface, &formatCount,
                                                                              nullptr) == VK_SUCCESS &&
                                         vkGetPhysicalDeviceSurfacePresentModesKHR(device, fSurface, &presentModeCount,
                                                                                   nullptr) == VK_SUCCESS &&
                                         formatCount > 0 && presentModeCount > 0;
        return graphicsQueueFamily != UINT32_MAX && presentQueueFamily != UINT32_MAX && hasSwapChainSupport;
    }

    void VKContext::CreateLogicalDevice()
    {
        std::array<VkDeviceQueueCreateInfo, 2> queueCreateInfos{};
        const auto firstFamily        = std::min(fGraphicsQueueFamily, fPresentQueueFamily);
        const auto lastFamily         = std::max(fGraphicsQueueFamily, fPresentQueueFamily);
        const uint32_t queueCount     = firstFamily == lastFamily ? 1 : 2;
        constexpr float queuePriority = 1.0f;
        for (uint32_t i = 0; i < queueCount; ++i)
        {
            queueCreateInfos[i] = {
                .sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                .queueFamilyIndex = i == 0 ? firstFamily : lastFamily,
                .queueCount       = 1,
                .pQueuePriorities = &queuePriority,
            };
        }
        const VkPhysicalDeviceFeatures deviceFeatures{};
        static constexpr std::array deviceExtensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
        const VkDeviceCreateInfo createInfo{
            .sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .queueCreateInfoCount    = queueCount,
            .pQueueCreateInfos       = queueCreateInfos.data(),
            .enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()),
            .ppEnabledExtensionNames = deviceExtensions.data(),
            .pEnabledFeatures        = &deviceFeatures,
        };

        VkDevice device;
        VkResult result = vkCreateDevice(fPhysicalDevice, &createInfo, nullptr, &device);
        CheckVkResult(result, "Failed to create logical device");
        fDevice = device;

        vkGetDeviceQueue(fDevice, fGraphicsQueueFamily, 0, &fGraphicsQueue);
        vkGetDeviceQueue(fDevice, fPresentQueueFamily, 0, &fPresentQueue);
    }

    bool VKContext::CreateSwapChain(int width, int height)
    {
        if (fSurface == VK_NULL_HANDLE)
            RecreateSurface();

        VkSurfaceCapabilitiesKHR capabilities{};
        VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(fPhysicalDevice, fSurface, &capabilities);
        if (result == VK_ERROR_SURFACE_LOST_KHR)
        {
            RecreateSurface();
            result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(fPhysicalDevice, fSurface, &capabilities);
        }
        CheckVkResult(result, "Failed to get Vulkan surface capabilities");

        VkExtent2D extent = capabilities.currentExtent;
        if (extent.width == UINT32_MAX)
        {
            extent.width  = std::clamp(static_cast<uint32_t>(std::max(width, 1)), capabilities.minImageExtent.width,
                                       capabilities.maxImageExtent.width);
            extent.height = std::clamp(static_cast<uint32_t>(std::max(height, 1)), capabilities.minImageExtent.height,
                                       capabilities.maxImageExtent.height);
        }

        // Win32 may report min/max/current extent all zero while minimized. It cannot host a swap chain yet.
        if (extent.width == 0 || extent.height == 0)
            return false;

        uint32_t formatCount{0};
        CheckVkResult(vkGetPhysicalDeviceSurfaceFormatsKHR(fPhysicalDevice, fSurface, &formatCount, nullptr),
                      "Failed to get Vulkan surface formats");
        if (formatCount == 0)
            throw std::runtime_error("The Vulkan surface does not expose any image formats");

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        CheckVkResult(vkGetPhysicalDeviceSurfaceFormatsKHR(fPhysicalDevice, fSurface, &formatCount, formats.data()),
                      "Failed to get Vulkan surface formats");

        uint32_t presentModeCount{0};
        CheckVkResult(vkGetPhysicalDeviceSurfacePresentModesKHR(fPhysicalDevice, fSurface, &presentModeCount, nullptr),
                      "Failed to get Vulkan present modes");
        if (presentModeCount == 0)
            throw std::runtime_error("The Vulkan surface does not expose any present modes");

        std::vector<VkPresentModeKHR> presentModes(presentModeCount);
        CheckVkResult(vkGetPhysicalDeviceSurfacePresentModesKHR(fPhysicalDevice, fSurface, &presentModeCount,
                                                                presentModes.data()),
                      "Failed to get Vulkan present modes");

        VkSurfaceFormatKHR surfaceFormat = formats[0];
        if (surfaceFormat.format == VK_FORMAT_UNDEFINED)
        {
            surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM;
        }
        else
        {
            for (const VkFormat preferredFormat : {VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM})
            {
                const auto format = std::find_if(formats.begin(), formats.end(),
                                                 [&](const VkSurfaceFormatKHR& candidate)
                                                 {
                                                     return candidate.format == preferredFormat &&
                                                            candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
                                                 });
                if (format != formats.end())
                {
                    surfaceFormat = *format;
                    break;
                }
            }
        }

        VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
        for (const auto& mode : {VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_FIFO_KHR})
        {
            if (std::find(presentModes.begin(), presentModes.end(), mode) != presentModes.end())
            {
                presentMode = mode;
                break;
            }
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType                    = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface                  = fSurface;
        createInfo.minImageCount            = imageCount;
        createInfo.imageFormat              = surfaceFormat.format;
        createInfo.imageColorSpace          = surfaceFormat.colorSpace;
        createInfo.imageExtent              = extent;
        createInfo.imageArrayLayers         = 1;
        createInfo.imageUsage               = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        const uint32_t queueFamilyIndices[] = {fGraphicsQueueFamily, fPresentQueueFamily};
        if (fGraphicsQueueFamily == fPresentQueueFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        }
        else
        {
            createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices   = queueFamilyIndices;
        }
        createInfo.preTransform = capabilities.currentTransform;
        for (const auto compositeAlpha :
             {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
              VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR})
        {
            if ((capabilities.supportedCompositeAlpha & compositeAlpha) != 0)
            {
                createInfo.compositeAlpha = compositeAlpha;
                break;
            }
        }
        createInfo.presentMode = presentMode;
        createInfo.clipped     = VK_TRUE;

        VkSwapchainKHR swapChain;
        result = vkCreateSwapchainKHR(fDevice, &createInfo, nullptr, &swapChain);
        CheckVkResult(result, "Failed to create swap chain");
        fSwapChain = swapChain;

        fSwapChainFormat = surfaceFormat.format;
        fSwapChainExtent = extent;

        uint32_t swapChainImageCount{0};
        CheckVkResult(vkGetSwapchainImagesKHR(fDevice, fSwapChain, &swapChainImageCount, nullptr),
                      "Failed to get Vulkan swap-chain images");
        fSwapChainImages.resize(swapChainImageCount);
        CheckVkResult(vkGetSwapchainImagesKHR(fDevice, fSwapChain, &swapChainImageCount, fSwapChainImages.data()),
                      "Failed to get Vulkan swap-chain images");

        fSwapChainImageViews.resize(swapChainImageCount);
        for (size_t i = 0; i < swapChainImageCount; ++i)
        {
            VkImageViewCreateInfo viewCreateInfo{};
            viewCreateInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewCreateInfo.image                           = fSwapChainImages[i];
            viewCreateInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            viewCreateInfo.format                          = fSwapChainFormat;
            viewCreateInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCreateInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCreateInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCreateInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            viewCreateInfo.subresourceRange.baseMipLevel   = 0;
            viewCreateInfo.subresourceRange.levelCount     = 1;
            viewCreateInfo.subresourceRange.baseArrayLayer = 0;
            viewCreateInfo.subresourceRange.layerCount     = 1;

            VkImageView imageView;
            result = vkCreateImageView(fDevice, &viewCreateInfo, nullptr, &imageView);
            CheckVkResult(result, "Failed to create image view");
            fSwapChainImageViews[i] = imageView;
        }
        return true;
    }

    void VKContext::CreateCommandPool()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = fGraphicsQueueFamily;

        VkCommandPool commandPool;
        VkResult result = vkCreateCommandPool(fDevice, &poolInfo, nullptr, &commandPool);
        CheckVkResult(result, "Failed to create command pool");
        fCommandPool = commandPool;
    }

    void VKContext::CleanupSwapChain()
    {
        for (auto imageView : fSwapChainImageViews)
        {
            if (imageView != VK_NULL_HANDLE)
                vkDestroyImageView(fDevice, imageView, nullptr);
        }
        fSwapChainImageViews.clear();
        fSwapChainImages.clear();

        if (fSwapChain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(fDevice, fSwapChain, nullptr);
            fSwapChain = VK_NULL_HANDLE;
        }
    }

    VkPhysicalDeviceProperties VKContext::GetPhysicalDeviceProperties() const
    {
        VkPhysicalDeviceProperties properties{};
        if (fPhysicalDevice != VK_NULL_HANDLE)
            vkGetPhysicalDeviceProperties(fPhysicalDevice, &properties);
        return properties;
    }
}  // namespace OIV
