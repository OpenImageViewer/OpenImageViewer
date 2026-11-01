#pragma once

#include "VKContext.h"
#include <filesystem>
#include <memory>
#include <vector>

namespace OIV
{
    class VKPipeline
    {
      public:

        struct CreateInfo
        {
            VkDevice device;
            VkExtent2D swapChainExtent;
            VkFormat swapChainFormat;
            std::filesystem::path vertexShaderPath;
            std::filesystem::path fragmentShaderPath;
            bool useTexture;
            uint32_t pushConstantSize;
            std::vector<VkImageView> swapChainImageViews;
        };

        VKPipeline(const CreateInfo& info);
        ~VKPipeline();

        VKPipeline(const VKPipeline&)            = delete;
        VKPipeline& operator=(const VKPipeline&) = delete;

        VkPipeline GetPipeline() const { return fPipeline; }
        VkPipelineLayout GetPipelineLayout() const { return fPipelineLayout; }
        VkDescriptorSetLayout GetDescriptorSetLayout() const { return fDescriptorSetLayout; }
        VkRenderPass GetRenderPass() const { return fRenderPass; }
        const std::vector<VkFramebuffer>& GetFramebuffers() const { return fFramebuffers; }
        VkSampler GetSampler(VkFilter filter) const;

        void DestroyFramebuffers();
        void RecreateFramebuffers(VkDevice device, const std::vector<VkImageView>& swapChainImageViews,
                                  VkExtent2D extent);
        void UpdateDescriptorSet(VkDevice device, VkDescriptorSet descriptorSet, VkImageView textureView,
                                 VkSampler sampler);

      private:

        VkShaderModule CreateShaderModule(VkDevice device, const std::filesystem::path& filePath);
        void CreateRenderPass(VkDevice device, VkFormat format);
        void CreateDescriptorSetLayout(VkDevice device, bool useTexture);
        void CreatePipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                  uint32_t pushConstantSize);
        void CreateGraphicsPipeline(VkDevice device, VkShaderModule vertexShader, VkShaderModule fragmentShader,
                                    VkRenderPass renderPass, VkPipelineLayout pipelineLayout);
        void CreateFramebuffers(VkDevice device, const std::vector<VkImageView>& swapChainImageViews,
                                VkExtent2D extent);
        VkSampler CreateSampler(VkFilter filter) const;
        void DestroyResources();

        VkDevice fDevice{VK_NULL_HANDLE};
        VkPipeline fPipeline{VK_NULL_HANDLE};
        VkPipelineLayout fPipelineLayout{VK_NULL_HANDLE};
        VkDescriptorSetLayout fDescriptorSetLayout{VK_NULL_HANDLE};
        VkRenderPass fRenderPass{VK_NULL_HANDLE};
        std::vector<VkFramebuffer> fFramebuffers;
        VkSampler fLinearSampler{VK_NULL_HANDLE};
        VkSampler fNearestSampler{VK_NULL_HANDLE};
    };

    using VKPipelineUniquePtr = std::unique_ptr<VKPipeline>;
}  // namespace OIV
