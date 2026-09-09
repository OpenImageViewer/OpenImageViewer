#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "VKCommon.h"
#include "VKRenderer.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <new>
#include <set>
#include <type_traits>
#include <utility>

namespace
{
    thread_local bool failNextHostAllocation{};
}

void* operator new(std::size_t size)
{
    if (std::exchange(failNextHostAllocation, false))
        throw std::bad_alloc();
    if (void* allocation = std::malloc(size == 0 ? 1 : size))
        return allocation;
    throw std::bad_alloc();
}

void operator delete(void* allocation) noexcept
{
    std::free(allocation);
}
void operator delete(void* allocation, std::size_t) noexcept
{
    std::free(allocation);
}

namespace
{
    enum class Failure
    {
        NoFailure,
        PoolBookkeeping,
        DescriptorCommit,
        FenceWait,
        Instance,
        Surface,
        Device,
        SwapChain,
        SwapChainImageView,
        CommandPool,
        RenderPass,
        DescriptorLayout,
        PipelineLayout,
        Framebuffer,
        Semaphore,
        Fence,
        ImageCreation,
        ImageMemory,
        ImageBinding,
        ImageView,
        StagingBuffer,
        StagingMemory,
        StagingBinding,
        Mapping,
        Recording,
        Submit,
        Present,
        SubmitMessage,
        PresentMessage
    };

    // The test executable links the real Vulkan backend against these deterministic Vulkan doubles.
    // Track externally observable ownership/synchronization, including waits that would hang on a real driver.
    struct Driver
    {
        uint32_t graphicsFamily = 0;
        uint32_t presentFamily  = 0;
        std::vector<uint32_t> createdQueueFamilies;
        std::set<uint32_t> descriptorBindingCounts;
        struct PhysicalDevice
        {
            uint32_t apiVersion{VK_API_VERSION_1_1};
            VkPhysicalDeviceType type{VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU};
            bool presentSupport{true};
            int propertyQueries{};
            int extensionQueries{};
        };
        std::vector<PhysicalDevice> devices{PhysicalDevice{}};
        bool zeroExtent{};
        VkExtent2D createdExtent{};
        int swapChainCreations{};
        int surfaceCreations{};
        int deviceWaits{};
        Failure failure{};
        uintptr_t nextHandle{1};
        int failAfter{};
        std::set<uintptr_t> objects;
        std::map<VkRenderPass, VkFormat> renderPassFormats;
        std::map<VkFramebuffer, VkFormat> framebufferFormats;
        std::map<VkPipeline, VkFormat> pipelineFormats;
        std::map<VkDescriptorPool, uint32_t> poolCapacity;
        std::map<VkDescriptorSet, VkDescriptorPool> descriptors;
        std::vector<VkImage> swapImages;
        std::deque<VkResult> capabilityResults;
        std::deque<VkResult> acquireResults;
        std::deque<VkResult> presentResults;
        VkFormat surfaceFormat{VK_FORMAT_B8G8R8A8_UNORM};
        VkFormat activeRenderFormat{VK_FORMAT_UNDEFINED};
        int poolCreations{};
        int pipelineCreations{};
        int acquireAttempts{};
        int renderPasses{};
        uint32_t uploadRowLength{};
        std::map<VkImage, VkExtent3D> images;
        std::set<VkImageView> views;
        std::set<VkDeviceMemory> memory;
        std::set<VkDeviceMemory> mappedMemory;
        std::set<VkBuffer> buffers;
        std::map<VkSemaphore, bool> semaphores;
        std::map<VkFence, bool> fences;
        std::array<std::byte, 64> mappedData{};
        bool allocatingImage{};
        bool imageAcquired{};
        int acquisitions{};
        int submissions{};
        int presentations{};
        int invalidAcquires{};
        int invalidWaits{};
        int emptySubmissions{};
        int bufferCreations{};
        int memoryAllocations{};
        int maps{};
        int unmaps{};
        int discardedUploads{};
        int readableUploads{};
        VkDeviceSize bufferAllocationSize{4};

        template <typename Handle>
        Handle MakeHandle()
        {
            if constexpr (std::is_pointer_v<Handle>)
                return reinterpret_cast<Handle>(nextHandle++);
            else
                return static_cast<Handle>(nextHandle++);
        }

        bool Fail(Failure point)
        {
            if (failure != point)
                return false;
            if (failAfter > 0)
            {
                --failAfter;
                return false;
            }
            failure = Failure::NoFailure;
            return true;
        }
        template <typename Handle>
        static uintptr_t Key(Handle handle)
        {
            if constexpr (std::is_pointer_v<Handle>)
                return reinterpret_cast<uintptr_t>(handle);
            else
                return static_cast<uintptr_t>(handle);
        }

        template <typename Handle>
        VkResult Create(Handle* handle, Failure point = Failure::NoFailure)
        {
            *handle = MakeHandle<Handle>();
            if (point != Failure::NoFailure && Fail(point))
                return VK_ERROR_OUT_OF_HOST_MEMORY;
            objects.insert(Key(*handle));
            return VK_SUCCESS;
        }

        template <typename Handle>
        void Destroy(Handle handle)
        {
            if (handle != VK_NULL_HANDLE)
                CHECK(objects.erase(Key(handle)) == 1);
        }

        static VkResult NextResult(std::deque<VkResult>& results)
        {
            if (results.empty())
                return VK_SUCCESS;
            const VkResult result = results.front();
            results.pop_front();
            return result;
        }
    } driver;

    class Renderable final : public OIV::IRenderable
    {
      public:

        Renderable()
        {
            auto item                                = std::make_shared<IMCodec::ImageItem>();
            item->descriptor.width                   = 1;
            item->descriptor.height                  = 1;
            item->descriptor.rowPitchInBytes         = 4;
            item->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
            item->data.Allocate(4);
            image = std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown);
        }

        double GetOpacity() const override { return 1.0; }
        LLUtils::PointF64 GetScale() const override { return {1.0, 1.0}; }
        LLUtils::PointF64 GetPosition() const override { return {}; }
        IMCodec::ImageSharedPtr GetImage() override { return image; }
        OIV_Filter_type GetFilterType() const override { return FT_Linear; }
        bool GetVisible() const override { return true; }
        uint32_t GetID() const override { return id; }
        OIV_Image_Render_mode GetImageRenderMode() const override { return IRM_MainImage; }
        bool GetIsImageDirty() const override { return dirty; }
        void ClearImageDirty() override { dirty = false; }
        void PreRender() override {}

        IMCodec::ImageSharedPtr image;
        bool dirty{true};
        uint32_t id{1};
    };
}  // namespace

extern "C"
{
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo*, const VkAllocationCallbacks*,
                                                    VkInstance* instance)
    {
        return driver.Create(instance, Failure::Instance);
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks*)
    {
        driver.Destroy(instance);
    }
    VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance, const char*)
    {
        return nullptr;
    }
#if defined(VK_USE_PLATFORM_WIN32_KHR)
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateWin32SurfaceKHR(VkInstance, const VkWin32SurfaceCreateInfoKHR*,
                                                           const VkAllocationCallbacks*, VkSurfaceKHR* surface)
#elif defined(VK_USE_PLATFORM_WAYLAND_KHR)
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateWaylandSurfaceKHR(VkInstance, const VkWaylandSurfaceCreateInfoKHR*,
                                                             const VkAllocationCallbacks*, VkSurfaceKHR* surface)
#else
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateXlibSurfaceKHR(VkInstance, const VkXlibSurfaceCreateInfoKHR*,
                                                          const VkAllocationCallbacks*, VkSurfaceKHR* surface)
#endif
    {
        ++driver.surfaceCreations;
        return driver.Create(surface, Failure::Surface);
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroySurfaceKHR(VkInstance, VkSurfaceKHR surface, const VkAllocationCallbacks*)
    {
        driver.Destroy(surface);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkEnumeratePhysicalDevices(VkInstance, uint32_t* count, VkPhysicalDevice* devices)
    {
        *count = static_cast<uint32_t>(driver.devices.size());
        if (devices != nullptr)
            for (size_t i = 0; i < driver.devices.size(); ++i)
                devices[i] = reinterpret_cast<VkPhysicalDevice>(&driver.devices[i]);
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceProperties(VkPhysicalDevice device,
                                                             VkPhysicalDeviceProperties* properties)
    {
        auto& physical = *reinterpret_cast<Driver::PhysicalDevice*>(device);
        ++physical.propertyQueries;
        *properties            = {};
        properties->apiVersion = physical.apiVersion;
        properties->deviceType = physical.type;
        std::strcpy(properties->deviceName, "Test GPU");
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(VkPhysicalDevice device, const char*,
                                                                        uint32_t* count,
                                                                        VkExtensionProperties* properties)
    {
        *count = 1;
        ++reinterpret_cast<Driver::PhysicalDevice*>(device)->extensionQueries;
        if (properties != nullptr)
            std::strcpy(properties->extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceQueueFamilyProperties(VkPhysicalDevice, uint32_t* count,
                                                                        VkQueueFamilyProperties* properties)
    {
        *count = std::max(driver.graphicsFamily, driver.presentFamily) + 1;
        if (properties != nullptr)
            for (uint32_t i = 0; i < *count; ++i)
                properties[i] = {.queueFlags = i == driver.graphicsFamily ? VkQueueFlags(VK_QUEUE_GRAPHICS_BIT) : 0u,
                                 .queueCount = 1};
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceSupportKHR(VkPhysicalDevice device, uint32_t family,
                                                                        VkSurfaceKHR, VkBool32* supported)
    {
        *supported = reinterpret_cast<Driver::PhysicalDevice*>(device)->presentSupport && family == driver.presentFamily
                         ? VK_TRUE
                         : VK_FALSE;
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(VkPhysicalDevice, const VkDeviceCreateInfo* info,
                                                  const VkAllocationCallbacks*, VkDevice* device)
    {
        driver.createdQueueFamilies.clear();
        for (uint32_t i = 0; i < info->queueCreateInfoCount; ++i)
        {
            const auto& queue = info->pQueueCreateInfos[i];
            CHECK(queue.queueCount == 1);
            CHECK(queue.pQueuePriorities[0] == 1.0f);
            driver.createdQueueFamilies.push_back(queue.queueFamilyIndex);
        }
        REQUIRE(info->enabledExtensionCount == 1);
        CHECK(std::string(info->ppEnabledExtensionNames[0]) == VK_KHR_SWAPCHAIN_EXTENSION_NAME);
        return driver.Create(device, Failure::Device);
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device, const VkAllocationCallbacks*)
    {
        driver.Destroy(device);
    }
    VKAPI_ATTR void VKAPI_CALL vkGetDeviceQueue(VkDevice, uint32_t, uint32_t, VkQueue* queue)
    {
        *queue = driver.MakeHandle<VkQueue>();
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VkPhysicalDevice, VkSurfaceKHR surface,
                                                                             VkSurfaceCapabilitiesKHR* capabilities)
    {
        REQUIRE(driver.objects.contains(Driver::Key(surface)));
        const VkResult result = Driver::NextResult(driver.capabilityResults);
        if (result != VK_SUCCESS)
            return result;
        *capabilities = {.minImageCount           = 2,
                         .maxImageCount           = 3,
                         .currentExtent           = {UINT32_MAX, UINT32_MAX},
                         .minImageExtent          = {1, 1},
                         .maxImageExtent          = {4096, 4096},
                         .maxImageArrayLayers     = 1,
                         .supportedTransforms     = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                         .currentTransform        = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
                         .supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
                         .supportedUsageFlags     = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
        if (driver.zeroExtent)
            capabilities->currentExtent = capabilities->minImageExtent = capabilities->maxImageExtent = {0, 0};
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(VkPhysicalDevice, VkSurfaceKHR, uint32_t* count,
                                                                        VkSurfaceFormatKHR* formats)
    {
        *count = 1;
        if (formats != nullptr)
            *formats = {driver.surfaceFormat, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfacePresentModesKHR(VkPhysicalDevice, VkSurfaceKHR,
                                                                             uint32_t* count, VkPresentModeKHR* modes)
    {
        *count = 1;
        if (modes != nullptr)
            *modes = VK_PRESENT_MODE_FIFO_KHR;
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice, const VkSwapchainCreateInfoKHR* info,
                                                        const VkAllocationCallbacks*, VkSwapchainKHR* swapchain)
    {
        REQUIRE(info->imageExtent.width > 0);
        REQUIRE(info->imageExtent.height > 0);
        driver.createdExtent = info->imageExtent;
        ++driver.swapChainCreations;
        const VkResult result = driver.Create(swapchain, Failure::SwapChain);
        if (result == VK_SUCCESS)
            driver.swapImages = {driver.MakeHandle<VkImage>(), driver.MakeHandle<VkImage>(),
                                 driver.MakeHandle<VkImage>()};
        return result;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice, VkSwapchainKHR swapchain, const VkAllocationCallbacks*)
    {
        driver.Destroy(swapchain);
        driver.imageAcquired = false;
        driver.swapImages.clear();
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkGetSwapchainImagesKHR(VkDevice, VkSwapchainKHR, uint32_t* count, VkImage* images)
    {
        *count = static_cast<uint32_t>(driver.swapImages.size());
        if (images != nullptr)
            std::copy(driver.swapImages.begin(), driver.swapImages.end(), images);
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo*,
                                                       const VkAllocationCallbacks*, VkCommandPool* pool)
    {
        return driver.Create(pool, Failure::CommandPool);
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyCommandPool(VkDevice, VkCommandPool pool, const VkAllocationCallbacks*)
    {
        driver.Destroy(pool);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateShaderModule(VkDevice, const VkShaderModuleCreateInfo*,
                                                        const VkAllocationCallbacks*, VkShaderModule* module)
    {
        return driver.Create(module);
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyShaderModule(VkDevice, VkShaderModule module, const VkAllocationCallbacks*)
    {
        driver.Destroy(module);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateRenderPass(VkDevice, const VkRenderPassCreateInfo* info,
                                                      const VkAllocationCallbacks*, VkRenderPass* pass)
    {
        const VkResult result = driver.Create(pass, Failure::RenderPass);
        if (result == VK_SUCCESS)
            driver.renderPassFormats[*pass] = info->pAttachments[0].format;
        return result;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyRenderPass(VkDevice, VkRenderPass pass, const VkAllocationCallbacks*)
    {
        driver.Destroy(pass);
        driver.renderPassFormats.erase(pass);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorSetLayout(VkDevice, const VkDescriptorSetLayoutCreateInfo* info,
                                                               const VkAllocationCallbacks*,
                                                               VkDescriptorSetLayout* layout)
    {
        driver.descriptorBindingCounts.insert(info->bindingCount);
        REQUIRE(info->bindingCount <= 1);
        if (info->bindingCount)
        {
            CHECK(info->pBindings[0].binding == 0);
            CHECK(info->pBindings[0].descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
            CHECK(info->pBindings[0].descriptorCount == 1);
            CHECK(info->pBindings[0].stageFlags == VK_SHADER_STAGE_FRAGMENT_BIT);
        }
        return driver.Create(layout, Failure::DescriptorLayout);
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorSetLayout(VkDevice, VkDescriptorSetLayout layout,
                                                            const VkAllocationCallbacks*)
    {
        driver.Destroy(layout);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreatePipelineLayout(VkDevice, const VkPipelineLayoutCreateInfo*,
                                                          const VkAllocationCallbacks*, VkPipelineLayout* layout)
    {
        return driver.Create(layout, Failure::PipelineLayout);
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyPipelineLayout(VkDevice, VkPipelineLayout layout, const VkAllocationCallbacks*)
    {
        driver.Destroy(layout);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateGraphicsPipelines(VkDevice, VkPipelineCache, uint32_t count,
                                                             const VkGraphicsPipelineCreateInfo* infos,
                                                             const VkAllocationCallbacks*, VkPipeline* pipelines)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            driver.Create(&pipelines[i]);
            driver.pipelineFormats[pipelines[i]] = driver.renderPassFormats.at(infos[i].renderPass);
            ++driver.pipelineCreations;
        }
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyPipeline(VkDevice, VkPipeline pipeline, const VkAllocationCallbacks*)
    {
        driver.Destroy(pipeline);
        driver.pipelineFormats.erase(pipeline);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateFramebuffer(VkDevice, const VkFramebufferCreateInfo* info,
                                                       const VkAllocationCallbacks*, VkFramebuffer* framebuffer)
    {
        const VkResult result = driver.Create(framebuffer, Failure::Framebuffer);
        if (result == VK_SUCCESS)
        {
            REQUIRE(driver.views.contains(info->pAttachments[0]));
            driver.framebufferFormats[*framebuffer] = driver.renderPassFormats.at(info->renderPass);
            CHECK(driver.framebufferFormats.at(*framebuffer) == driver.surfaceFormat);
        }
        return result;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyFramebuffer(VkDevice, VkFramebuffer framebuffer, const VkAllocationCallbacks*)
    {
        driver.Destroy(framebuffer);
        driver.framebufferFormats.erase(framebuffer);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateSampler(VkDevice, const VkSamplerCreateInfo*, const VkAllocationCallbacks*,
                                                   VkSampler* sampler)
    {
        return driver.Create(sampler);
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroySampler(VkDevice, VkSampler sampler, const VkAllocationCallbacks*)
    {
        driver.Destroy(sampler);
    }
    VKAPI_ATTR void VKAPI_CALL vkUpdateDescriptorSets(VkDevice, uint32_t, const VkWriteDescriptorSet*, uint32_t,
                                                      const VkCopyDescriptorSet*)
    {
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice, const VkImageCreateInfo* info, const VkAllocationCallbacks*,
                                                 VkImage* image)
    {
        *image = driver.MakeHandle<VkImage>();
        if (driver.Fail(Failure::ImageCreation))
            return VK_ERROR_OUT_OF_DEVICE_MEMORY;
        driver.images.emplace(*image, info->extent);
        driver.allocatingImage = true;
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyImage(VkDevice, VkImage image, const VkAllocationCallbacks*)
    {
        if (image != VK_NULL_HANDLE)
            CHECK(driver.images.erase(image) == 1);
    }
    VKAPI_ATTR void VKAPI_CALL vkGetImageMemoryRequirements(VkDevice, VkImage, VkMemoryRequirements* requirements)
    {
        *requirements = {.size = 4, .alignment = 4, .memoryTypeBits = 1};
    }
    VKAPI_ATTR void VKAPI_CALL vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice,
                                                                   VkPhysicalDeviceMemoryProperties* properties)
    {
        *properties                              = {};
        properties->memoryTypeCount              = 1;
        properties->memoryTypes[0].propertyFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const VkAllocationCallbacks*,
                                                    VkDeviceMemory* memory)
    {
        // Error outputs are undefined in Vulkan. Poison them instead of preserving a friendly null value.
        *memory = driver.MakeHandle<VkDeviceMemory>();
        if (driver.Fail(driver.allocatingImage ? Failure::ImageMemory : Failure::StagingMemory))
            return VK_ERROR_OUT_OF_DEVICE_MEMORY;
        driver.memory.insert(*memory);
        ++driver.memoryAllocations;
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkFreeMemory(VkDevice, VkDeviceMemory memory, const VkAllocationCallbacks*)
    {
        if (memory != VK_NULL_HANDLE)
        {
            CHECK_FALSE(driver.mappedMemory.contains(memory));
            CHECK(driver.memory.erase(memory) == 1);
        }
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(VkDevice, VkImage, VkDeviceMemory, VkDeviceSize)
    {
        return driver.Fail(Failure::ImageBinding) ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(VkDevice, const VkImageViewCreateInfo* info,
                                                     const VkAllocationCallbacks*, VkImageView* view)
    {
        *view = driver.MakeHandle<VkImageView>();
        if (driver.Fail(driver.images.contains(info->image) ? Failure::ImageView : Failure::SwapChainImageView))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        driver.views.insert(*view);
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyImageView(VkDevice, VkImageView view, const VkAllocationCallbacks*)
    {
        if (view != VK_NULL_HANDLE)
            CHECK(driver.views.erase(view) == 1);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice, const VkBufferCreateInfo*, const VkAllocationCallbacks*,
                                                  VkBuffer* buffer)
    {
        *buffer = driver.MakeHandle<VkBuffer>();
        if (driver.Fail(Failure::StagingBuffer))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        driver.buffers.insert(*buffer);
        ++driver.bufferCreations;
        driver.allocatingImage = false;
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyBuffer(VkDevice, VkBuffer buffer, const VkAllocationCallbacks*)
    {
        if (buffer != VK_NULL_HANDLE)
            CHECK(driver.buffers.erase(buffer) == 1);
    }
    VKAPI_ATTR void VKAPI_CALL vkGetBufferMemoryRequirements(VkDevice, VkBuffer, VkMemoryRequirements* requirements)
    {
        *requirements = {.size = driver.bufferAllocationSize, .alignment = 4, .memoryTypeBits = 1};
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, VkDeviceSize)
    {
        return driver.Fail(Failure::StagingBinding) ? VK_ERROR_OUT_OF_DEVICE_MEMORY : VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkMapMemory(VkDevice, VkDeviceMemory memory, VkDeviceSize, VkDeviceSize,
                                               VkMemoryMapFlags, void** data)
    {
        *data = reinterpret_cast<void*>(1);
        if (driver.Fail(Failure::Mapping))
            return VK_ERROR_MEMORY_MAP_FAILED;
        *data = driver.mappedData.data();
        driver.mappedMemory.insert(memory);
        ++driver.maps;
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkUnmapMemory(VkDevice, VkDeviceMemory memory)
    {
        CHECK(driver.mappedMemory.erase(memory) == 1);
        ++driver.unmaps;
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdPipelineBarrier(VkCommandBuffer, VkPipelineStageFlags source,
                                                    VkPipelineStageFlags destination, VkDependencyFlags, uint32_t,
                                                    const VkMemoryBarrier*, uint32_t, const VkBufferMemoryBarrier*,
                                                    uint32_t count, const VkImageMemoryBarrier* barriers)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            const auto& barrier = barriers[i];
            if (barrier.newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                CHECK(barrier.oldLayout == VK_IMAGE_LAYOUT_UNDEFINED);
                CHECK(barrier.dstAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
                CHECK(destination == VK_PIPELINE_STAGE_TRANSFER_BIT);
                ++driver.discardedUploads;
            }
            else
            {
                CHECK(barrier.oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                CHECK(barrier.newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                CHECK(barrier.srcAccessMask == VK_ACCESS_TRANSFER_WRITE_BIT);
                CHECK(barrier.dstAccessMask == VK_ACCESS_SHADER_READ_BIT);
                CHECK(source == VK_PIPELINE_STAGE_TRANSFER_BIT);
                CHECK(destination == VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
                ++driver.readableUploads;
            }
        }
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdCopyBufferToImage(VkCommandBuffer, VkBuffer, VkImage image, VkImageLayout layout,
                                                      uint32_t count, const VkBufferImageCopy* regions)
    {
        REQUIRE(count == 1);
        driver.uploadRowLength = regions[0].bufferRowLength;
        const auto& extent     = driver.images.at(image);
        CHECK(layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CHECK(regions[0].imageOffset.x == 0);
        CHECK(regions[0].imageOffset.y == 0);
        CHECK(regions[0].imageOffset.z == 0);
        CHECK(regions[0].imageExtent.width == extent.width);
        CHECK(regions[0].imageExtent.height == extent.height);
        CHECK(regions[0].imageExtent.depth == extent.depth);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*,
                                                            VkCommandBuffer* command)
    {
        *command = driver.MakeHandle<VkCommandBuffer>();
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkFreeCommandBuffers(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*) {}
    VKAPI_ATTR VkResult VKAPI_CALL vkResetCommandBuffer(VkCommandBuffer, VkCommandBufferResetFlags)
    {
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*)
    {
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkEndCommandBuffer(VkCommandBuffer)
    {
        return driver.Fail(Failure::Recording) ? VK_ERROR_OUT_OF_HOST_MEMORY : VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdBeginRenderPass(VkCommandBuffer, const VkRenderPassBeginInfo* info,
                                                    VkSubpassContents)
    {
        REQUIRE(driver.framebufferFormats.contains(info->framebuffer));
        driver.activeRenderFormat = driver.renderPassFormats.at(info->renderPass);
        CHECK(driver.framebufferFormats.at(info->framebuffer) == driver.activeRenderFormat);
        ++driver.renderPasses;
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdEndRenderPass(VkCommandBuffer) {}
    VKAPI_ATTR void VKAPI_CALL vkCmdSetViewport(VkCommandBuffer, uint32_t, uint32_t, const VkViewport*) {}
    VKAPI_ATTR void VKAPI_CALL vkCmdSetScissor(VkCommandBuffer, uint32_t, uint32_t, const VkRect2D*) {}
    VKAPI_ATTR void VKAPI_CALL vkCmdBindPipeline(VkCommandBuffer, VkPipelineBindPoint, VkPipeline pipeline)
    {
        CHECK(driver.pipelineFormats.at(pipeline) == driver.activeRenderFormat);
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdBindDescriptorSets(VkCommandBuffer, VkPipelineBindPoint, VkPipelineLayout, uint32_t,
                                                       uint32_t, const VkDescriptorSet*, uint32_t, const uint32_t*)
    {
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdPushConstants(VkCommandBuffer, VkPipelineLayout, VkShaderStageFlags, uint32_t,
                                                  uint32_t, const void*)
    {
    }
    VKAPI_ATTR void VKAPI_CALL vkCmdDraw(VkCommandBuffer, uint32_t, uint32_t, uint32_t, uint32_t) {}
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateDescriptorPool(VkDevice, const VkDescriptorPoolCreateInfo* info,
                                                          const VkAllocationCallbacks*, VkDescriptorPool* pool)
    {
        driver.Create(pool);
        driver.poolCapacity[*pool] = info->maxSets;
        ++driver.poolCreations;
        if (driver.Fail(Failure::PoolBookkeeping))
            failNextHostAllocation = true;
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyDescriptorPool(VkDevice, VkDescriptorPool pool, const VkAllocationCallbacks*)
    {
        driver.Destroy(pool);
        driver.poolCapacity.erase(pool);
        std::erase_if(driver.descriptors, [=](const auto& item) { return item.second == pool; });
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkAllocateDescriptorSets(VkDevice, const VkDescriptorSetAllocateInfo* info,
                                                            VkDescriptorSet* descriptor)
    {
        auto& capacity = driver.poolCapacity.at(info->descriptorPool);
        *descriptor    = VK_NULL_HANDLE;
        if (capacity == 0)
            return VK_ERROR_OUT_OF_POOL_MEMORY;
        --capacity;
        *descriptor                     = driver.MakeHandle<VkDescriptorSet>();
        driver.descriptors[*descriptor] = info->descriptorPool;
        if (driver.Fail(Failure::DescriptorCommit))
            failNextHostAllocation = true;
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkFreeDescriptorSets(VkDevice, VkDescriptorPool pool, uint32_t count,
                                                        const VkDescriptorSet* descriptors)
    {
        for (uint32_t i = 0; i < count; ++i)
        {
            CHECK(driver.descriptors.at(descriptors[i]) == pool);
            driver.descriptors.erase(descriptors[i]);
            ++driver.poolCapacity.at(pool);
        }
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(VkDevice, const VkSemaphoreCreateInfo*,
                                                     const VkAllocationCallbacks*, VkSemaphore* semaphore)
    {
        *semaphore = driver.MakeHandle<VkSemaphore>();
        if (driver.Fail(Failure::Semaphore))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        driver.semaphores[*semaphore] = false;
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroySemaphore(VkDevice, VkSemaphore semaphore, const VkAllocationCallbacks*)
    {
        CHECK(driver.semaphores.erase(semaphore) == 1);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice, const VkFenceCreateInfo* info, const VkAllocationCallbacks*,
                                                 VkFence* fence)
    {
        *fence = driver.MakeHandle<VkFence>();
        if (driver.Fail(Failure::Fence))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        driver.fences[*fence] = (info->flags & VK_FENCE_CREATE_SIGNALED_BIT) != 0;
        return VK_SUCCESS;
    }
    VKAPI_ATTR void VKAPI_CALL vkDestroyFence(VkDevice, VkFence fence, const VkAllocationCallbacks*)
    {
        CHECK(driver.fences.erase(fence) == 1);
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice, uint32_t count, const VkFence* fences)
    {
        for (uint32_t i = 0; i < count; ++i)
            driver.fences.at(fences[i]) = false;
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice, uint32_t count, const VkFence* fences, VkBool32, uint64_t)
    {
        if (driver.Fail(Failure::FenceWait))
            return VK_ERROR_DEVICE_LOST;
        for (uint32_t i = 0; i < count; ++i)
        {
            if (!driver.fences.at(fences[i]))
            {
                ++driver.invalidWaits;
                return VK_TIMEOUT;
            }
        }
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice, VkSwapchainKHR swapchain, uint64_t,
                                                         VkSemaphore semaphore, VkFence, uint32_t* index)
    {
        REQUIRE(driver.objects.contains(Driver::Key(swapchain)));
        ++driver.acquireAttempts;
        const VkResult result = Driver::NextResult(driver.acquireResults);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            return result;
        if (driver.semaphores.at(semaphore) || driver.imageAcquired)
        {
            ++driver.invalidAcquires;
            return VK_ERROR_UNKNOWN;
        }
        driver.semaphores.at(semaphore) = true;
        driver.imageAcquired            = true;
        ++driver.acquisitions;
        *index = 0;
        return result;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit(VkQueue, uint32_t count, const VkSubmitInfo* submits, VkFence fence)
    {
        if (driver.Fail(Failure::Submit))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        for (uint32_t i = 0; i < count; ++i)
        {
            for (uint32_t j = 0; j < submits[i].waitSemaphoreCount; ++j)
            {
                auto& signal = driver.semaphores.at(submits[i].pWaitSemaphores[j]);
                CHECK(signal);
                signal = false;
            }
            for (uint32_t j = 0; j < submits[i].signalSemaphoreCount; ++j)
            {
                auto& signal = driver.semaphores.at(submits[i].pSignalSemaphores[j]);
                CHECK_FALSE(signal);
                signal = true;
            }
            if (submits[i].commandBufferCount == 0)
                ++driver.emptySubmissions;
        }
        if (fence != VK_NULL_HANDLE)
            driver.fences.at(fence) = true;
        ++driver.submissions;
        if (driver.Fail(Failure::SubmitMessage))
            failNextHostAllocation = true;
        return VK_SUCCESS;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue, const VkPresentInfoKHR* info)
    {
        failNextHostAllocation = false;
        if (driver.Fail(Failure::Present))
            return VK_ERROR_OUT_OF_HOST_MEMORY;
        for (uint32_t i = 0; i < info->waitSemaphoreCount; ++i)
        {
            auto& signal = driver.semaphores.at(info->pWaitSemaphores[i]);
            CHECK(signal);
            signal = false;
        }
        CHECK(driver.imageAcquired);
        driver.imageAcquired  = false;
        const VkResult result = Driver::NextResult(driver.presentResults);
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            return result;
        ++driver.presentations;
        if (driver.Fail(Failure::PresentMessage))
            failNextHostAllocation = true;
        return result;
    }
    VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device)
    {
        CHECK(driver.objects.contains(Driver::Key(device)));
        ++driver.deviceWaits;
        return VK_SUCCESS;
    }
}

TEST_CASE("Failed Vulkan texture construction releases every completed allocation", "[vulkan][failure]")
{
    driver         = {};
    driver.failure = GENERATE(Failure::ImageCreation, Failure::ImageMemory, Failure::ImageBinding, Failure::ImageView);
    REQUIRE_THROWS_AS(OIV::VKTexture(driver.MakeHandle<VkDevice>(), driver.MakeHandle<VkPhysicalDevice>(), 1, 1,
                                     VK_FORMAT_R8G8B8A8_UNORM),
                      OIV::VKException);
    CHECK(driver.images.empty());
    CHECK(driver.views.empty());
    CHECK(driver.memory.empty());
}

TEST_CASE("Vulkan redraw retries an acquired frame after resource or submission failure", "[vulkan][failure]")
{
    driver                = {};
    const Failure failure = GENERATE(Failure::ImageCreation, Failure::ImageMemory, Failure::ImageBinding,
                                     Failure::ImageView, Failure::StagingBuffer, Failure::StagingMemory,
                                     Failure::StagingBinding, Failure::Mapping, Failure::Recording, Failure::Submit,
                                     Failure::Present);
    Renderable image;
    {
        OIV::VKRenderer renderer;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        REQUIRE(renderer.AddRenderable(&image) == 0);
        driver.failure = failure;
        REQUIRE_THROWS_AS(renderer.Redraw(), OIV::VKException);
        // A new redraw must also render scene changes made after a failed present.
        image.dirty = true;
        REQUIRE(renderer.Redraw() == 0);
        CHECK(driver.invalidAcquires == 0);
        CHECK(driver.invalidWaits == 0);
        const int completedFrames = failure == Failure::Present ? 2 : 1;
        CHECK(driver.acquisitions == completedFrames);
        CHECK(driver.submissions == completedFrames);
        CHECK(driver.presentations == completedFrames);
        CHECK_FALSE(image.dirty);
        REQUIRE(renderer.Redraw() == 0);
        CHECK(driver.presentations == completedFrames + 1);
        REQUIRE(renderer.RemoveRenderable(&image) == 0);
    }
    CHECK(driver.images.empty());
    CHECK(driver.views.empty());
    CHECK(driver.memory.empty());
    CHECK(driver.buffers.empty());
    CHECK(driver.semaphores.empty());
    CHECK(driver.fences.empty());
}

TEST_CASE("Vulkan failed submission permits image removal and swapchain retirement", "[vulkan][failure]")
{
    driver = {};
    Renderable image;
    {
        OIV::VKRenderer renderer;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        REQUIRE(renderer.AddRenderable(&image) == 0);
        driver.failure = Failure::Submit;
        REQUIRE_THROWS_AS(renderer.Redraw(), OIV::VKException);
        REQUIRE(renderer.RemoveRenderable(&image) == 0);
        CHECK(driver.invalidWaits == 0);
        SECTION("Resize before retry")
        {
            REQUIRE(renderer.SetViewParams({.uViewportSize = {64, 64}}) == 0);
            REQUIRE(renderer.Redraw() == 0);
            CHECK(driver.invalidAcquires == 0);
            CHECK(driver.presentations == 1);
        }
        SECTION("Destroy before retry") {}
    }
    CHECK(driver.emptySubmissions == 1);
    CHECK(driver.images.empty());
    CHECK(driver.memory.empty());
    CHECK(driver.buffers.empty());
}

TEST_CASE("Successful Vulkan submission and presentation commit without host allocation", "[vulkan][failure]")
{
    driver                = {};
    const Failure failure = GENERATE(Failure::SubmitMessage, Failure::PresentMessage);
    OIV::VKRenderer renderer;
    Renderable image;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    REQUIRE(renderer.AddRenderable(&image) == 0);
    driver.failure = failure;
    bool completed = false;
    try
    {
        renderer.Redraw();
        completed = true;
    }
    catch (const std::bad_alloc&)
    {
    }
    failNextHostAllocation = false;
    REQUIRE(completed);
    REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.invalidAcquires == 0);
    CHECK(driver.invalidWaits == 0);
    CHECK(driver.presentations == 2);
    REQUIRE(renderer.RemoveRenderable(&image) == 0);
}

TEST_CASE("Vulkan reuses one bounded mapped upload buffer after frame completion", "[vulkan][upload]")
{
    driver               = {};
    const bool oversized = GENERATE(false, true);
    if (oversized)
        driver.bufferAllocationSize = 32 * 1024 * 1024;
    constexpr int Frames = 10;
    {
        OIV::VKRenderer renderer;
        Renderable image;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        REQUIRE(renderer.AddRenderable(&image) == 0);
        for (int i = 0; i < Frames; ++i)
        {
            image.dirty = true;
            REQUIRE(renderer.Redraw() == 0);
        }
        const int uploads = oversized ? Frames : 1;
        CHECK(driver.bufferCreations == uploads);
        CHECK(driver.maps == uploads);
        CHECK(driver.memoryAllocations == uploads + 1);
        CHECK(driver.presentations == Frames);
        CHECK(driver.discardedUploads == Frames);
        CHECK(driver.readableUploads == Frames);
        REQUIRE(renderer.RemoveRenderable(&image) == 0);
        CHECK(driver.buffers.size() == (oversized ? 0 : 1));
    }
    CHECK(driver.buffers.empty());
    CHECK(driver.memory.empty());
    CHECK(driver.mappedMemory.empty());
    CHECK(driver.maps == driver.unmaps);
}

TEST_CASE("Vulkan startup cleans only successfully created handles", "[vulkan][failure]")
{
    driver         = {};
    driver.failure = GENERATE(Failure::Instance, Failure::Device, Failure::SwapChain, Failure::SwapChainImageView,
                              Failure::CommandPool, Failure::RenderPass, Failure::DescriptorLayout,
                              Failure::PipelineLayout, Failure::Framebuffer, Failure::Semaphore, Failure::Fence);
    if (driver.failure == Failure::Semaphore || driver.failure == Failure::Framebuffer ||
        driver.failure == Failure::SwapChainImageView)
        driver.failAfter = GENERATE(0, 1, 2);
    {
        OIV::VKRenderer renderer;
        REQUIRE_THROWS_AS(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}), OIV::VKException);
    }
    CHECK(driver.objects.empty());
    CHECK(driver.views.empty());
    CHECK(driver.semaphores.empty());
    CHECK(driver.fences.empty());
}

TEST_CASE("Vulkan retries complete reconstruction after a partial resize failure", "[vulkan][failure]")
{
    driver             = {};
    bool formatChanged = false;
    Failure failure;
    SECTION("Unchanged surface format")
    {
        failure = GENERATE(Failure::SwapChain, Failure::SwapChainImageView, Failure::Framebuffer, Failure::Semaphore);
    }
    SECTION("Changed surface format")
    {
        formatChanged = true;
        failure = GENERATE(Failure::SwapChain, Failure::SwapChainImageView, Failure::Framebuffer, Failure::Semaphore,
                           Failure::RenderPass, Failure::DescriptorLayout, Failure::PipelineLayout);
    }
    {
        OIV::VKRenderer renderer;
        Renderable image;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        REQUIRE(renderer.AddRenderable(&image) == 0);
        REQUIRE(renderer.Redraw() == 0);
        if (formatChanged)
            driver.surfaceFormat = VK_FORMAT_R8G8B8A8_UNORM;
        driver.failure = failure;
        if (failure != Failure::SwapChain)
            driver.failAfter = 1;
        const OIV::ViewParameters view{.uViewportSize = {64, 64}};
        REQUIRE(renderer.SetViewParams(view) == 0);
        REQUIRE_THROWS_AS(renderer.Redraw(), OIV::VKException);
        // OIV supplies the same requested size on the next refresh.
        REQUIRE(renderer.SetViewParams(view) == 0);
        REQUIRE(renderer.Redraw() == 0);
        CHECK(driver.presentations == 2);
        CHECK(driver.activeRenderFormat == driver.surfaceFormat);
        CHECK(driver.pipelineFormats.size() == 3);
        if (!formatChanged)
            CHECK(driver.pipelineCreations == 3);
        REQUIRE(renderer.RemoveRenderable(&image) == 0);
    }
    CHECK(driver.objects.empty());
    CHECK(driver.views.empty());
}

TEST_CASE("Vulkan retains the need to replace a surface after replacement fails", "[vulkan][failure]")
{
    driver = {};
    {
        OIV::VKRenderer renderer;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        driver.acquireResults = {VK_ERROR_SURFACE_LOST_KHR};
        driver.failure        = Failure::Surface;
        REQUIRE_THROWS_AS(renderer.Redraw(), OIV::VKException);
        REQUIRE(renderer.Redraw() == 0);
        CHECK(driver.presentations == 1);
    }
    CHECK(driver.objects.empty());
}

TEST_CASE("Vulkan completes an event-driven redraw after surface invalidation", "[vulkan][recovery]")
{
    driver                      = {};
    const VkResult invalidation = GENERATE(VK_ERROR_OUT_OF_DATE_KHR, VK_ERROR_SURFACE_LOST_KHR);
    const bool duringPresent    = GENERATE(false, true);
    OIV::VKRenderer renderer;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    (duringPresent ? driver.presentResults : driver.acquireResults).push_back(invalidation);
    REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.presentations == 1);
    CHECK(driver.acquireAttempts == 2);
    CHECK(driver.renderPasses == (duringPresent ? 2 : 1));
}

TEST_CASE("Vulkan stops retrying a persistently invalid surface", "[vulkan][recovery]")
{
    driver = {};
    OIV::VKRenderer renderer;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    driver.acquireResults = {VK_ERROR_OUT_OF_DATE_KHR, VK_ERROR_OUT_OF_DATE_KHR};
    REQUIRE_THROWS_AS(renderer.Redraw(), std::runtime_error);
    CHECK(driver.acquireAttempts == 2);
    REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.presentations == 1);
}

TEST_CASE("Vulkan descriptor pools stay bounded when renderables are replaced", "[vulkan][descriptors]")
{
    driver = {};
    {
        OIV::VKRenderer renderer;
        std::array<Renderable, 33> images;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        for (size_t i = 0; i < images.size(); ++i)
        {
            images[i].id = static_cast<uint32_t>(i + 1);
            REQUIRE(renderer.AddRenderable(&images[i]) == 0);
        }
        for (size_t i = 0; i < 320; ++i)
        {
            Renderable& image = images[i % images.size()];
            REQUIRE(renderer.RemoveRenderable(&image) == 0);
            REQUIRE(renderer.AddRenderable(&image) == 0);
        }
        CHECK(driver.descriptors.size() == images.size());
        CHECK(driver.poolCreations == 2);
        for (auto& image : images)
            REQUIRE(renderer.RemoveRenderable(&image) == 0);
    }
    CHECK(driver.objects.empty());
    CHECK(driver.descriptors.empty());
}

TEST_CASE("Vulkan accepts a suboptimal surface without duplicating the frame", "[vulkan][recovery]")
{
    driver = {};
    OIV::VKRenderer renderer;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    driver.acquireResults = {VK_SUBOPTIMAL_KHR};
    driver.presentResults = {VK_SUBOPTIMAL_KHR};
    REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.acquireAttempts == 1);
    CHECK(driver.presentations == 1);
    CHECK(driver.renderPasses == 1);
}

TEST_CASE("Vulkan retries a surface replacement that failed during a capabilities query", "[vulkan][failure]")
{
    driver = {};
    OIV::VKRenderer renderer;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    driver.capabilityResults = {VK_ERROR_SURFACE_LOST_KHR};
    driver.failure           = Failure::Surface;
    REQUIRE(renderer.SetViewParams({.uViewportSize = {64, 64}}) == 0);
    REQUIRE_THROWS_AS(renderer.Redraw(), OIV::VKException);
    REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.presentations == 1);
}

TEST_CASE("Vulkan image removal still releases resources after device loss", "[vulkan][failure]")
{
    driver = {};
    OIV::VKRenderer renderer;
    Renderable image;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    REQUIRE(renderer.AddRenderable(&image) == 0);
    REQUIRE(renderer.Redraw() == 0);
    driver.failure = Failure::FenceWait;
    REQUIRE_NOTHROW(renderer.RemoveRenderable(&image));
    CHECK(driver.images.empty());
    CHECK(driver.descriptors.empty());
}

TEST_CASE("Vulkan registration releases a pool when host bookkeeping fails", "[vulkan][failure]")
{
    driver = {};
    {
        OIV::VKRenderer renderer;
        Renderable image;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        driver.failure = Failure::PoolBookkeeping;
        REQUIRE_THROWS_AS(renderer.AddRenderable(&image), std::bad_alloc);
        CHECK(driver.poolCapacity.empty());
        REQUIRE(renderer.AddRenderable(&image) == 0);
        REQUIRE(renderer.RemoveRenderable(&image) == 0);
    }
    CHECK(driver.objects.empty());
}

TEST_CASE("Vulkan registration commits without allocation after allocating its descriptor", "[vulkan][failure]")
{
    driver = {};
    OIV::VKRenderer renderer;
    Renderable image;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    driver.failure = Failure::DescriptorCommit;
    bool added     = false;
    try
    {
        renderer.AddRenderable(&image);
        added = true;
    }
    catch (const std::bad_alloc&)
    {
    }
    failNextHostAllocation = false;
    REQUIRE(added);
    CHECK(driver.descriptors.size() == 1);
    REQUIRE(renderer.RemoveRenderable(&image) == 0);
}

TEST_CASE("Vulkan uploads raw RGBA rows with packed or padded byte pitches", "[vulkan][upload]")
{
    driver               = {};
    const uint32_t pitch = GENERATE(8U, 9U, 12U);
    std::vector<std::byte> pixels(pitch * 2, std::byte{0xff});
    for (uint32_t row = 0; row < 2; ++row)
        for (uint32_t column = 0; column < 8; ++column)
            pixels[row * pitch + column] = static_cast<std::byte>(row * 8 + column);
    auto item                                = std::make_shared<IMCodec::ImageItem>();
    item->descriptor.width                   = 2;
    item->descriptor.height                  = 2;
    item->descriptor.rowPitchInBytes         = pitch;
    item->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
    item->data.Allocate(pixels.size());
    item->data.Write(pixels.data(), 0, pixels.size());
    Renderable image;
    image.image = std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown);
    OIV::VKRenderer renderer;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    REQUIRE(renderer.AddRenderable(&image) == 0);
    REQUIRE(renderer.Redraw() == 0);
    REQUIRE(driver.uploadRowLength >= 2);
    for (uint32_t row = 0; row < 2; ++row)
        for (uint32_t column = 0; column < 8; ++column)
            CHECK(driver.mappedData[row * driver.uploadRowLength * 4 + column] == pixels[row * pitch + column]);
    CHECK(driver.bufferCreations == 1);
    REQUIRE(renderer.RemoveRenderable(&image) == 0);
}

TEST_CASE("Vulkan selects a presentable 1.1 device with stable enumeration indices", "[vulkan][selection]")
{
    driver         = {};
    driver.devices = {
        {.apiVersion = VK_API_VERSION_1_0},
        {.type = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU},
        {.apiVersion = VK_API_VERSION_1_2},
        {.apiVersion = VK_API_VERSION_1_3},
    };
    int requested = -1;
    int expected  = 2;
    SECTION("Automatic selection skips old discrete GPUs") {}
    SECTION("Explicit index selects the integrated GPU")
    {
        requested = 1;
        expected  = 1;
    }
    SECTION("First suitable device is used when discrete GPUs cannot present")
    {
        driver.devices[2].presentSupport = false;
        driver.devices[3].presentSupport = false;
        expected                         = 1;
    }
    {
        OIV::VKRenderer renderer;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = requested}) == 0);
        CHECK(renderer.GetSelectedGPUIndex() == expected);
        CHECK(driver.devices[0].extensionQueries == 0);
        CHECK(driver.devices[expected].propertyQueries == 1);
        if (requested == 1)
        {
            CHECK(driver.devices[0].propertyQueries == 0);
            CHECK(driver.devices[2].propertyQueries == 0);
        }
    }
    CHECK(driver.objects.empty());
}

TEST_CASE("Vulkan rejects unsupported explicit GPUs without silently selecting another", "[vulkan][selection]")
{
    driver = {};
    driver.devices.resize(2);
    SECTION("Vulkan 1.0")
    {
        driver.devices[0].apiVersion = VK_API_VERSION_1_0;
    }
    SECTION("No window presentation")
    {
        driver.devices[0].presentSupport = false;
    }
    {
        OIV::VKRenderer renderer;
        REQUIRE_THROWS_AS(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = 0}),
                          std::runtime_error);
        CHECK(driver.devices[1].propertyQueries == 0);
        CHECK(driver.swapChainCreations == 0);
    }
    CHECK(driver.objects.empty());
}

TEST_CASE("Vulkan rejects automatic selection when every GPU is below 1.1", "[vulkan][selection]")
{
    driver                       = {};
    driver.devices[0].apiVersion = VK_API_VERSION_1_0;
    {
        OIV::VKRenderer renderer;
        REQUIRE_THROWS_AS(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}),
                          std::runtime_error);
        CHECK(driver.devices[0].extensionQueries == 0);
    }
    CHECK(driver.objects.empty());
}

TEST_CASE("Vulkan coalesces view changes before rebuilding the swap chain", "[vulkan][recovery]")
{
    driver = {};
    OIV::VKRenderer renderer;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    REQUIRE(renderer.Redraw() == 0);
    for (int size : {64, 128, 256})
        REQUIRE(renderer.SetViewParams({.uViewportSize = {size, size}}) == 0);
    CHECK(driver.swapChainCreations == 1);
    CHECK(driver.deviceWaits == 0);
    REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.swapChainCreations == 2);
    CHECK(driver.deviceWaits == 1);
    CHECK(driver.createdExtent.width == 256);
    CHECK(driver.createdExtent.height == 256);
    REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.swapChainCreations == 2);
    CHECK(driver.deviceWaits == 1);
}

TEST_CASE("Vulkan defers zero-sized views and restores the same previous size", "[vulkan][recovery]")
{
    driver = {};
    OIV::VKRenderer renderer;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    REQUIRE(renderer.SetViewParams({.uViewportSize = {64, 64}}) == 0);
    REQUIRE(renderer.Redraw() == 0);
    const int creations = driver.swapChainCreations;
    const int waits     = driver.deviceWaits;
    driver.zeroExtent   = true;
    REQUIRE(renderer.SetViewParams({.uViewportSize = {0, 0}}) == 0);
    for (int i = 0; i < 3; ++i)
        REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.acquireAttempts == 1);
    CHECK(driver.swapChainCreations == creations);
    CHECK(driver.deviceWaits == waits);
    driver.zeroExtent = false;
    REQUIRE(renderer.SetViewParams({.uViewportSize = {64, 64}}) == 0);
    REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.presentations == 2);
    CHECK(driver.swapChainCreations == creations + 1);
}

TEST_CASE("Vulkan initializes images after a zero-extent startup is restored", "[vulkan][recovery]")
{
    driver                        = {};
    driver.zeroExtent             = true;
    const bool registerBeforeInit = GENERATE(false, true);
    {
        Renderable image;
        OIV::VKRenderer renderer;
        if (registerBeforeInit)
            REQUIRE(renderer.AddRenderable(&image) == 0);
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        if (!registerBeforeInit)
            REQUIRE(renderer.AddRenderable(&image) == 0);
        for (int i = 0; i < 3; ++i)
            REQUIRE(renderer.Redraw() == 0);
        CHECK(driver.acquireAttempts == 0);
        CHECK(driver.swapChainCreations == 0);
        CHECK(driver.pipelineCreations == 0);
        CHECK(driver.deviceWaits == 0);
        driver.zeroExtent = false;
        REQUIRE(renderer.Redraw() == 0);
        CHECK(driver.presentations == 1);
        CHECK(driver.descriptors.size() == 1);
        CHECK_FALSE(image.dirty);
        REQUIRE(renderer.RemoveRenderable(&image) == 0);
    }
    CHECK(driver.objects.empty());
    CHECK(driver.views.empty());
    CHECK(driver.memory.empty());
    CHECK(driver.semaphores.empty());
}

TEST_CASE("Vulkan keeps recovery pending while the native surface has zero extent", "[vulkan][recovery]")
{
    driver                      = {};
    const VkResult invalidation = GENERATE(VK_ERROR_OUT_OF_DATE_KHR, VK_ERROR_SURFACE_LOST_KHR);
    const bool duringPresent    = GENERATE(false, true);
    OIV::VKRenderer renderer;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    REQUIRE(renderer.Redraw() == 0);
    driver.zeroExtent = true;
    (duringPresent ? driver.presentResults : driver.acquireResults).push_back(invalidation);
    REQUIRE(renderer.Redraw() == 0);
    const int acquisitions = driver.acquireAttempts;
    const int waits        = driver.deviceWaits;
    const int surfaces     = driver.surfaceCreations;
    for (int i = 0; i < 3; ++i)
        REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.acquireAttempts == acquisitions);
    CHECK(driver.deviceWaits == waits);
    CHECK(driver.swapChainCreations == 1);
    CHECK(driver.surfaceCreations == surfaces);
    CHECK(surfaces == (invalidation == VK_ERROR_SURFACE_LOST_KHR ? 2 : 1));
    driver.zeroExtent = false;
    REQUIRE(renderer.Redraw() == 0);
    CHECK(driver.presentations == 2);
    CHECK(driver.swapChainCreations == 2);
    CHECK(driver.invalidWaits == 0);
    CHECK(driver.invalidAcquires == 0);
}

TEST_CASE("Vulkan reconstructs after a previously failed presentation becomes invalid", "[vulkan][recovery]")
{
    driver                      = {};
    const VkResult invalidation = GENERATE(VK_ERROR_OUT_OF_DATE_KHR, VK_SUBOPTIMAL_KHR, VK_ERROR_SURFACE_LOST_KHR);
    OIV::VKRenderer renderer;
    Renderable image;
    REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
    REQUIRE(renderer.AddRenderable(&image) == 0);
    driver.failure = Failure::Present;
    REQUIRE_THROWS_AS(renderer.Redraw(), OIV::VKException);
    driver.presentResults.push_back(invalidation);
    image.dirty = true;
    REQUIRE(renderer.Redraw() == 0);
    CHECK_FALSE(image.dirty);
    CHECK(driver.presentations == (invalidation == VK_SUBOPTIMAL_KHR ? 2 : 1));
    CHECK(driver.swapChainCreations == 2);
    CHECK(driver.invalidWaits == 0);
    CHECK(driver.invalidAcquires == 0);
    REQUIRE(renderer.RemoveRenderable(&image) == 0);
}

TEST_CASE("Vulkan retries failed first pipeline creation after zero-extent startup", "[vulkan][failure]")
{
    driver                = {};
    driver.zeroExtent     = true;
    const Failure failure = GENERATE(Failure::RenderPass, Failure::DescriptorLayout, Failure::PipelineLayout,
                                     Failure::Framebuffer, Failure::Semaphore, Failure::PoolBookkeeping);
    {
        OIV::VKRenderer renderer;
        Renderable image;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        REQUIRE(renderer.AddRenderable(&image) == 0);
        driver.zeroExtent = false;
        driver.failure    = failure;
        if (failure == Failure::Framebuffer)
            driver.failAfter = 1;
        REQUIRE_THROWS(renderer.Redraw());
        REQUIRE(driver.failure == Failure::NoFailure);
        REQUIRE(renderer.Redraw() == 0);
        CHECK(driver.presentations == 1);
        CHECK(driver.descriptors.size() == 1);
        CHECK_FALSE(image.dirty);
        REQUIRE(renderer.RemoveRenderable(&image) == 0);
    }
    CHECK(driver.objects.empty());
    CHECK(driver.views.empty());
    CHECK(driver.memory.empty());
    CHECK(driver.semaphores.empty());
}

TEST_CASE("Vulkan adapter selectors do not silently fall back", "[vulkan][selection]")
{
    driver = {};
    SECTION("Index outside enumeration")
    {
        OIV::VKRenderer renderer;
        REQUIRE_THROWS_AS(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = 99}),
                          std::invalid_argument);
    }
    SECTION("Unknown name")
    {
        OIV::VKRenderer renderer;
        REQUIRE_THROWS_AS(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .adapterName = "missing adapter"}),
                          std::runtime_error);
    }
    SECTION("Case-insensitive exact name")
    {
        OIV::VKRenderer renderer;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .adapterName = "test gpu"}) == 0);
        CHECK(renderer.GetSelectedGPUIndex() == 0);
    }
    CHECK(driver.objects.empty());
}

TEST_CASE("Vulkan uses bounded queue storage for shared and distinct families", "[vulkan][selection]")
{
    driver                = {};
    const auto families   = GENERATE(std::pair{0u, 0u}, std::pair{0u, 1u}, std::pair{1u, 0u});
    driver.graphicsFamily = families.first;
    driver.presentFamily  = families.second;
    {
        OIV::VKRenderer renderer;
        REQUIRE(renderer.Init({.container = 1, .dataPath = OIV_TEXT("."), .gpuIndex = -1}) == 0);
        const auto expected = families.first == families.second ? std::vector<uint32_t>{0}
                                                                : std::vector<uint32_t>{0, 1};
        CHECK(driver.createdQueueFamilies == expected);
        CHECK(driver.descriptorBindingCounts == std::set<uint32_t>{0, 1});
    }
    CHECK(driver.objects.empty());
}

TEST_CASE("Vulkan reports each adapter's acceleration and enumeration index without selecting it",
          "[vulkan][selection]")
{
    driver         = {};
    driver.devices = {{.type = VK_PHYSICAL_DEVICE_TYPE_CPU},
                      {.type = VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU},
                      {.type = VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU},
                      {.type = VK_PHYSICAL_DEVICE_TYPE_OTHER}};
    OIV::VKRenderer renderer;
    const auto adapters = renderer.EnumerateAdapters();
    REQUIRE(adapters.size() == 4);
    CHECK(adapters[0].acceleration == OIV::Acceleration::Software);
    CHECK(adapters[1].acceleration == OIV::Acceleration::Hardware);
    CHECK(adapters[2].preferred);
    CHECK(adapters[3].acceleration == OIV::Acceleration::Unknown);
    for (size_t i = 0; i < adapters.size(); ++i)
        CHECK(adapters[i].index == static_cast<int>(i));
    CHECK(renderer.GetSelectedGPUIndex() == -1);
    CHECK(driver.objects.empty());
}
