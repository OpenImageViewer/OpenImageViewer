#pragma once

#include "VKContext.h"
#include <cstdint>
#include <memory>

namespace OIV
{
    struct VKUploadBuffer
    {
        explicit VKUploadBuffer(VkDevice device) : device(device) {}
        ~VKUploadBuffer();
        VKUploadBuffer(const VKUploadBuffer&)            = delete;
        VKUploadBuffer& operator=(const VKUploadBuffer&) = delete;

        VkDevice device;
        VkBuffer buffer{VK_NULL_HANDLE};
        VkDeviceMemory memory{VK_NULL_HANDLE};
        VkDeviceSize size{};
        VkDeviceSize allocationSize{};
        void* mapped{};
    };

    class VKTexture
    {
      public:

        VKTexture(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height, VkFormat format);
        ~VKTexture();

        VKTexture(const VKTexture&)            = delete;
        VKTexture& operator=(const VKTexture&) = delete;

        VkImageView GetImageView() const { return fImageView; }
        uint32_t GetWidth() const { return fWidth; }
        uint32_t GetHeight() const { return fHeight; }
        void Upload(VkCommandBuffer commandBuffer, const void* data, uint32_t rowPitch,
                    std::unique_ptr<VKUploadBuffer>& cache);
        void ReleaseUploadResources(std::unique_ptr<VKUploadBuffer>& cache);

      private:

        void DestroyResources();
        void CreateImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height,
                         VkFormat format);
        void CreateImageView(VkDevice device, VkFormat format);
        uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter, VkMemoryPropertyFlags properties);
        void TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
                                   VkImageLayout newLayout);
        void CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, uint32_t rowPitch, uint32_t width,
                               uint32_t height);
        std::unique_ptr<VKUploadBuffer> CreateUploadBuffer(VkDeviceSize size);
        VkDevice fDevice{VK_NULL_HANDLE};
        VkPhysicalDevice fPhysicalDevice{VK_NULL_HANDLE};
        VkImage fImage{VK_NULL_HANDLE};
        VkDeviceMemory fImageMemory{VK_NULL_HANDLE};
        VkImageView fImageView{VK_NULL_HANDLE};
        std::unique_ptr<VKUploadBuffer> fUploadBuffer;
        uint32_t fWidth{0};
        uint32_t fHeight{0};
    };
}  // namespace OIV
