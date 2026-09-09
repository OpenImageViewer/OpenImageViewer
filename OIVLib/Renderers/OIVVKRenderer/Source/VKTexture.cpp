#include "VKTexture.h"
#include "VKCommon.h"
#include <cstring>

namespace OIV
{
    VKUploadBuffer::~VKUploadBuffer()
    {
        if (mapped != nullptr)
            vkUnmapMemory(device, memory);
        if (buffer != VK_NULL_HANDLE)
            vkDestroyBuffer(device, buffer, nullptr);
        if (memory != VK_NULL_HANDLE)
            vkFreeMemory(device, memory, nullptr);
    }

    VKTexture::VKTexture(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height,
                         VkFormat format)
        : fDevice(device), fPhysicalDevice(physicalDevice), fWidth(width), fHeight(height)
    {
        try
        {
            CreateImage(device, physicalDevice, width, height, format);
            CreateImageView(device, format);
        }
        catch (...)
        {
            // A failed constructor does not run the destructor.
            DestroyResources();
            throw;
        }
    }

    VKTexture::~VKTexture()
    {
        DestroyResources();
    }

    void VKTexture::DestroyResources()
    {
        if (fDevice != VK_NULL_HANDLE)
        {
            fUploadBuffer.reset();
            if (fImageView != VK_NULL_HANDLE)
                vkDestroyImageView(fDevice, fImageView, nullptr);
            if (fImage != VK_NULL_HANDLE)
                vkDestroyImage(fDevice, fImage, nullptr);
            if (fImageMemory != VK_NULL_HANDLE)
                vkFreeMemory(fDevice, fImageMemory, nullptr);
        }
    }

    void VKTexture::Upload(VkCommandBuffer commandBuffer, const void* data, uint32_t rowPitch,
                           std::unique_ptr<VKUploadBuffer>& cache)
    {
        constexpr uint32_t BytesPerTexel = 4;
        const uint32_t packedRowPitch    = fWidth * BytesPerTexel;
        const uint32_t sourceRowPitch    = rowPitch > 0 ? rowPitch : packedRowPitch;
        if (sourceRowPitch < packedRowPitch)
            throw std::invalid_argument("The Vulkan texture row pitch is invalid for an RGBA8 image");

        const uint32_t uploadRowPitch = sourceRowPitch % BytesPerTexel == 0 ? sourceRowPitch : packedRowPitch;
        const VkDeviceSize bufferSize = static_cast<VkDeviceSize>(uploadRowPitch) * fHeight;
        fUploadBuffer                 = cache != nullptr && cache->size >= bufferSize ? std::move(cache)
                                                                                      : CreateUploadBuffer(bufferSize);
        if (uploadRowPitch == sourceRowPitch)
            std::memcpy(fUploadBuffer->mapped, data, static_cast<size_t>(bufferSize));
        else
        {
            // Vulkan bufferRowLength is in texels; pack byte pitches it cannot represent directly.
            for (uint32_t row = 0; row < fHeight; ++row)
                std::memcpy(static_cast<std::byte*>(fUploadBuffer->mapped) + static_cast<size_t>(row) * uploadRowPitch,
                            static_cast<const std::byte*>(data) + static_cast<size_t>(row) * sourceRowPitch,
                            packedRowPitch);
        }

        // Every upload replaces the full image after prior rendering completes, so discard its old contents.
        TransitionImageLayout(commandBuffer, fImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
        CopyBufferToImage(commandBuffer, fUploadBuffer->buffer, uploadRowPitch, fWidth, fHeight);
        TransitionImageLayout(commandBuffer, fImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                              VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void VKTexture::ReleaseUploadResources(std::unique_ptr<VKUploadBuffer>& cache)
    {
        // The renderer calls this only after submission completion (or for an unsubmitted frame).
        // Retain at most one small allocation per renderer; larger allocations stay transient.
        constexpr VkDeviceSize MaxCachedBytes = 16 * 1024 * 1024;
        if (fUploadBuffer != nullptr && fUploadBuffer->allocationSize <= MaxCachedBytes &&
            (cache == nullptr || cache->size < fUploadBuffer->size))
            cache = std::move(fUploadBuffer);
        fUploadBuffer.reset();
    }

    void VKTexture::CreateImage(VkDevice device, VkPhysicalDevice physicalDevice, uint32_t width, uint32_t height,
                                VkFormat format)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = format;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.flags         = 0;

        VkImage image;
        VkResult result = vkCreateImage(device, &imageInfo, nullptr, &image);
        CheckVkResult(result, "Failed to create image");
        fImage = image;

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, fImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(physicalDevice, memRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory memory;
        result = vkAllocateMemory(device, &allocInfo, nullptr, &memory);
        CheckVkResult(result, "Failed to allocate image memory");
        fImageMemory = memory;

        CheckVkResult(vkBindImageMemory(device, fImage, fImageMemory, 0), "Failed to bind Vulkan image memory");
    }

    void VKTexture::CreateImageView(VkDevice device, VkFormat format)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                           = fImage;
        viewInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                          = format;
        viewInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel   = 0;
        viewInfo.subresourceRange.levelCount     = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount     = 1;

        VkImageView view;
        VkResult result = vkCreateImageView(device, &viewInfo, nullptr, &view);
        CheckVkResult(result, "Failed to create image view");
        fImageView = view;
    }

    uint32_t VKTexture::FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeFilter,
                                       VkMemoryPropertyFlags properties)
    {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i)
        {
            if ((typeFilter & (1U << i)) != 0 &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
                return i;
        }

        throw std::runtime_error("Failed to find suitable memory type");
    }

    void VKTexture::TransitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkImageLayout oldLayout,
                                          VkImageLayout newLayout)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout                       = oldLayout;
        barrier.newLayout                       = newLayout;
        barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
        barrier.image                           = image;
        barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel   = 0;
        barrier.subresourceRange.levelCount     = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount     = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage           = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
                 newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage           = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else
        {
            throw std::invalid_argument("Unsupported layout transition");
        }

        vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    }

    void VKTexture::CopyBufferToImage(VkCommandBuffer commandBuffer, VkBuffer buffer, uint32_t rowPitch, uint32_t width,
                                      uint32_t height)
    {
        constexpr uint32_t BytesPerTexel = 4;
        VkBufferImageCopy region{};
        region.bufferOffset                    = 0;
        region.bufferRowLength                 = rowPitch / BytesPerTexel;
        region.bufferImageHeight               = 0;
        region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel       = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount     = 1;
        region.imageOffset                     = {0, 0, 0};
        region.imageExtent                     = {width, height, 1};

        vkCmdCopyBufferToImage(commandBuffer, buffer, fImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    }

    std::unique_ptr<VKUploadBuffer> VKTexture::CreateUploadBuffer(VkDeviceSize size)
    {
        auto upload = std::make_unique<VKUploadBuffer>(fDevice);
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size        = size;
        bufferInfo.usage       = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer;
        VkResult result = vkCreateBuffer(fDevice, &bufferInfo, nullptr, &buffer);
        CheckVkResult(result, "Failed to create buffer");
        upload->buffer = buffer;
        upload->size   = size;

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(fDevice, buffer, &memRequirements);
        upload->allocationSize = memRequirements.size;

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize  = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(fPhysicalDevice, memRequirements.memoryTypeBits,
                                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        VkDeviceMemory memory;
        result = vkAllocateMemory(fDevice, &allocInfo, nullptr, &memory);
        CheckVkResult(result, "Failed to allocate buffer memory");
        upload->memory = memory;

        CheckVkResult(vkBindBufferMemory(fDevice, buffer, memory, 0), "Failed to bind Vulkan buffer memory");
        void* mapped;
        CheckVkResult(vkMapMemory(fDevice, memory, 0, size, 0, &mapped), "Failed to map Vulkan staging memory");
        upload->mapped = mapped;
        return upload;
    }
}  // namespace OIV
