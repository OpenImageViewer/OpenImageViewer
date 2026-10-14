#pragma once

#include <cstdint>
#include <RendererCore/RendererOptions.h>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
#endif

#include <vulkan/vulkan.h>

namespace OIV
{
    class VKContext
    {
      public:

        struct CreateParams
        {
            std::uintptr_t window;
            void* nativeDisplay;
            int width;
            int height;
            int gpuIndex;
            const char* adapterName = nullptr;
        };

        VKContext()                            = default;
        VKContext(const VKContext&)            = delete;
        VKContext& operator=(const VKContext&) = delete;
        ~VKContext();

        static std::vector<RendererAdapter> EnumerateAdapters();
        const RendererAdapter& GetSelectedAdapter() const { return fSelectedAdapter; }
        void Init(const CreateParams& params);
        void Purge();
        bool RecreateSwapChain(int width, int height, bool recoverSurface);

        VkPhysicalDevice GetPhysicalDevice() const { return fPhysicalDevice; }
        VkDevice GetDevice() const { return fDevice; }
        VkSwapchainKHR GetSwapChain() const { return fSwapChain; }
        VkQueue GetGraphicsQueue() const { return fGraphicsQueue; }
        VkQueue GetPresentQueue() const { return fPresentQueue; }
        VkFormat GetSwapChainFormat() const { return fSwapChainFormat; }
        VkExtent2D GetSwapChainExtent() const { return fSwapChainExtent; }
        VkCommandPool GetCommandPool() const { return fCommandPool; }
        const std::vector<VkImage>& GetSwapChainImages() const { return fSwapChainImages; }
        const std::vector<VkImageView>& GetSwapChainImageViews() const { return fSwapChainImageViews; }

      private:

        void CreateInstance();
        void CreateSurface(void* nativeDisplay, std::uintptr_t window);
        void PickPhysicalDevice(int gpuIndex, const char* adapterName);
        bool IsPhysicalDeviceSuitable(VkPhysicalDevice device, uint32_t& graphicsQueueFamily,
                                      uint32_t& presentQueueFamily) const;
        void CreateLogicalDevice();
        bool CreateSwapChain(int width, int height);
        void CreateCommandPool();
        void CleanupSwapChain();
        void RecreateSurface();

        VkInstance fInstance{VK_NULL_HANDLE};
        VkPhysicalDevice fPhysicalDevice{VK_NULL_HANDLE};
        VkDevice fDevice{VK_NULL_HANDLE};
        VkSurfaceKHR fSurface{VK_NULL_HANDLE};
        VkSwapchainKHR fSwapChain{VK_NULL_HANDLE};
        VkFormat fSwapChainFormat{VK_FORMAT_UNDEFINED};
        VkExtent2D fSwapChainExtent{0, 0};
        VkCommandPool fCommandPool{VK_NULL_HANDLE};

        uint32_t fGraphicsQueueFamily{UINT32_MAX};
        uint32_t fPresentQueueFamily{UINT32_MAX};
        VkQueue fGraphicsQueue{VK_NULL_HANDLE};
        VkQueue fPresentQueue{VK_NULL_HANDLE};

        std::vector<VkImage> fSwapChainImages;
        std::vector<VkImageView> fSwapChainImageViews;

        RendererAdapter fSelectedAdapter;

        std::uintptr_t fWindow{};
#ifdef __linux__
        void* fNativeDisplay{nullptr};
#endif
    };
}  // namespace OIV
