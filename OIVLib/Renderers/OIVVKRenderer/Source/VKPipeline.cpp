#include "VKPipeline.h"
#include "VKCommon.h"
#include <array>
#include <cstring>
#include <fstream>
#include <stdexcept>

namespace OIV
{
    VkShaderModule VKPipeline::CreateShaderModule(VkDevice device, const std::filesystem::path& filePath)
    {
        std::ifstream file(filePath, std::ios::ate | std::ios::binary);
        if (!file.is_open())
            throw std::runtime_error("Failed to open shader file: " + filePath.string());

        const std::streamoff fileSize = file.tellg();
        if (fileSize <= 0 || fileSize % static_cast<std::streamoff>(sizeof(uint32_t)) != 0)
            throw std::runtime_error("Invalid SPIR-V shader file: " + filePath.string());

        file.seekg(0);

        std::vector<uint32_t> buffer(static_cast<size_t>(fileSize) / sizeof(uint32_t));
        if (!file.read(reinterpret_cast<char*>(buffer.data()), fileSize))
            throw std::runtime_error("Failed to read SPIR-V shader file: " + filePath.string());

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = static_cast<size_t>(fileSize);
        createInfo.pCode    = buffer.data();

        VkShaderModule shaderModule;
        VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
        CheckVkResult(result, "Failed to create shader module");

        return shaderModule;
    }

    void VKPipeline::CreateRenderPass(VkDevice device, VkFormat format)
    {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format         = format;
        colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments    = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass    = 0;
        dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments    = &colorAttachment;
        renderPassInfo.subpassCount    = 1;
        renderPassInfo.pSubpasses      = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies   = &dependency;

        VkRenderPass renderPass;
        VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
        CheckVkResult(result, "Failed to create render pass");
        fRenderPass = renderPass;
    }

    void VKPipeline::CreateDescriptorSetLayout(VkDevice device, bool useTexture)
    {
        static constexpr VkDescriptorSetLayoutBinding samplerBinding{
            .binding         = 0,
            .descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags      = VK_SHADER_STAGE_FRAGMENT_BIT,
        };
        const VkDescriptorSetLayoutCreateInfo layoutInfo{
            .sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
            .bindingCount = useTexture ? 1u : 0u,
            .pBindings    = useTexture ? &samplerBinding : nullptr,
        };

        VkDescriptorSetLayout layout;
        VkResult result = vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout);
        CheckVkResult(result, "Failed to create descriptor set layout");
        fDescriptorSetLayout = layout;
    }

    void VKPipeline::CreatePipelineLayout(VkDevice device, VkDescriptorSetLayout descriptorSetLayout,
                                          uint32_t pushConstantSize)
    {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset     = 0;
        pushConstantRange.size       = pushConstantSize;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount         = 1;
        pipelineLayoutInfo.pSetLayouts            = &descriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges    = &pushConstantRange;

        VkPipelineLayout layout;
        VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &layout);
        CheckVkResult(result, "Failed to create pipeline layout");
        fPipelineLayout = layout;
    }

    void VKPipeline::CreateGraphicsPipeline(VkDevice device, VkShaderModule vertexShader, VkShaderModule fragmentShader,
                                            VkRenderPass renderPass, VkPipelineLayout pipelineLayout)
    {
        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage  = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertexShader;
        shaderStages[0].pName  = "main";
        shaderStages[1].sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragmentShader;
        shaderStages[1].pName  = "main";

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount   = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount  = 1;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable        = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
        rasterizer.cullMode                = VK_CULL_MODE_NONE;
        rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable         = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f;
        rasterizer.depthBiasClamp          = 0.0f;
        rasterizer.depthBiasSlopeFactor    = 0.0f;
        rasterizer.lineWidth               = 1.0f;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable  = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.blendEnable         = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        colorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable     = VK_FALSE;
        colorBlending.logicOp           = VK_LOGIC_OP_COPY;
        colorBlending.attachmentCount   = 1;
        colorBlending.pAttachments      = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f;
        colorBlending.blendConstants[1] = 0.0f;
        colorBlending.blendConstants[2] = 0.0f;
        colorBlending.blendConstants[3] = 0.0f;

        VkPipelineDynamicStateCreateInfo dynamicState{};
        constexpr std::array dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        dynamicState.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount     = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates        = dynamicStates.data();

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount          = 2;
        pipelineInfo.pStages             = shaderStages;
        pipelineInfo.pVertexInputState   = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState      = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState   = &multisampling;
        pipelineInfo.pDepthStencilState  = nullptr;
        pipelineInfo.pColorBlendState    = &colorBlending;
        pipelineInfo.pDynamicState       = &dynamicState;
        pipelineInfo.layout              = pipelineLayout;
        pipelineInfo.renderPass          = renderPass;
        pipelineInfo.subpass             = 0;
        pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

        VkResult result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &fPipeline);
        CheckVkResult(result, "Failed to create graphics pipeline");
    }

    void VKPipeline::CreateFramebuffers(VkDevice device, const std::vector<VkImageView>& swapChainImageViews,
                                        VkExtent2D extent)
    {
        fFramebuffers.resize(swapChainImageViews.size());

        for (size_t i = 0; i < swapChainImageViews.size(); ++i)
        {
            VkImageView attachments[] = {swapChainImageViews[i]};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = fRenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments    = attachments;
            framebufferInfo.width           = extent.width;
            framebufferInfo.height          = extent.height;
            framebufferInfo.layers          = 1;

            VkFramebuffer framebuffer;
            VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &framebuffer);
            CheckVkResult(result, "Failed to create framebuffer");
            fFramebuffers[i] = framebuffer;
        }
    }

    void VKPipeline::UpdateDescriptorSet(VkDevice device, VkDescriptorSet descriptorSet, VkImageView textureView,
                                         VkSampler sampler)
    {
        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView   = textureView;
        imageInfo.sampler     = sampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType            = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet           = descriptorSet;
        descriptorWrite.dstBinding       = 0;
        descriptorWrite.dstArrayElement  = 0;
        descriptorWrite.descriptorType   = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount  = 1;
        descriptorWrite.pImageInfo       = &imageInfo;
        descriptorWrite.pBufferInfo      = nullptr;
        descriptorWrite.pTexelBufferView = nullptr;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
    }

    VKPipeline::VKPipeline(const CreateInfo& info) : fDevice(info.device)
    {
        VkShaderModule vertexShader{VK_NULL_HANDLE};
        VkShaderModule fragmentShader{VK_NULL_HANDLE};
        try
        {
            vertexShader   = CreateShaderModule(info.device, info.vertexShaderPath);
            fragmentShader = CreateShaderModule(info.device, info.fragmentShaderPath);

            CreateRenderPass(info.device, info.swapChainFormat);
            CreateDescriptorSetLayout(info.device, info.useTexture);
            CreatePipelineLayout(info.device, fDescriptorSetLayout, info.pushConstantSize);
            CreateGraphicsPipeline(info.device, vertexShader, fragmentShader, fRenderPass, fPipelineLayout);
            if (!info.swapChainImageViews.empty())
                CreateFramebuffers(info.device, info.swapChainImageViews, info.swapChainExtent);

            if (info.useTexture)
            {
                fLinearSampler  = CreateSampler(VK_FILTER_LINEAR);
                fNearestSampler = CreateSampler(VK_FILTER_NEAREST);
            }
        }
        catch (...)
        {
            if (fragmentShader != VK_NULL_HANDLE)
                vkDestroyShaderModule(info.device, fragmentShader, nullptr);
            if (vertexShader != VK_NULL_HANDLE)
                vkDestroyShaderModule(info.device, vertexShader, nullptr);
            DestroyResources();
            throw;
        }

        vkDestroyShaderModule(info.device, vertexShader, nullptr);
        vkDestroyShaderModule(info.device, fragmentShader, nullptr);
    }

    VkSampler VKPipeline::CreateSampler(VkFilter filter) const
    {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter               = filter;
        samplerInfo.minFilter               = filter;
        samplerInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipLodBias              = 0.0f;
        samplerInfo.anisotropyEnable        = VK_FALSE;
        samplerInfo.maxAnisotropy           = 1.0f;
        samplerInfo.compareEnable           = VK_FALSE;
        samplerInfo.compareOp               = VK_COMPARE_OP_ALWAYS;
        samplerInfo.minLod                  = 0.0f;
        samplerInfo.maxLod                  = 0.0f;
        samplerInfo.borderColor             = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;

        VkSampler sampler{VK_NULL_HANDLE};
        VkResult result = vkCreateSampler(fDevice, &samplerInfo, nullptr, &sampler);
        CheckVkResult(result, "Failed to create sampler");
        return sampler;
    }

    VkSampler VKPipeline::GetSampler(VkFilter filter) const
    {
        return filter == VK_FILTER_NEAREST ? fNearestSampler : fLinearSampler;
    }

    VKPipeline::~VKPipeline()
    {
        DestroyResources();
    }

    void VKPipeline::DestroyResources()
    {
        if (fDevice != VK_NULL_HANDLE)
        {
            if (fNearestSampler != VK_NULL_HANDLE)
                vkDestroySampler(fDevice, fNearestSampler, nullptr);
            if (fLinearSampler != VK_NULL_HANDLE)
                vkDestroySampler(fDevice, fLinearSampler, nullptr);
            DestroyFramebuffers();
            if (fPipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(fDevice, fPipeline, nullptr);
            if (fPipelineLayout != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(fDevice, fPipelineLayout, nullptr);
            if (fDescriptorSetLayout != VK_NULL_HANDLE)
                vkDestroyDescriptorSetLayout(fDevice, fDescriptorSetLayout, nullptr);
            if (fRenderPass != VK_NULL_HANDLE)
                vkDestroyRenderPass(fDevice, fRenderPass, nullptr);
        }
    }

    void VKPipeline::RecreateFramebuffers(VkDevice device, const std::vector<VkImageView>& swapChainImageViews,
                                          VkExtent2D extent)
    {
        DestroyFramebuffers();
        CreateFramebuffers(device, swapChainImageViews, extent);
    }

    void VKPipeline::DestroyFramebuffers()
    {
        for (auto framebuffer : fFramebuffers)
        {
            if (framebuffer != VK_NULL_HANDLE)
                vkDestroyFramebuffer(fDevice, framebuffer, nullptr);
        }
        fFramebuffers.clear();
    }
}  // namespace OIV
