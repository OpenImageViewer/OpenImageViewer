#include "VKRenderer.h"
#include "VKCommon.h"
#include <ImageUtil/ImageUtil.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace OIV
{
    namespace
    {
        std::array<float, 4> ToFloatColor(LLUtils::Color color)
        {
            const LLUtils::ColorF32 floatColor = static_cast<LLUtils::ColorF32>(color);
            return floatColor.channels;
        }

        std::filesystem::path FindShaderFile(const std::filesystem::path& fileName,
                                             const std::filesystem::path& cachePath)
        {
            std::error_code ec;
            std::filesystem::path exeDir = LLUtils::PlatformUtility::GetExeFolder();
            std::filesystem::path local  = exeDir / "ShaderCache" / fileName;
            if (std::filesystem::exists(local, ec))
                return local;

            std::filesystem::path sharedCache = exeDir.parent_path() / "ShaderCache" / fileName;
            if (std::filesystem::exists(sharedCache, ec))
                return sharedCache;

            return cachePath / fileName;
        }
    }  // namespace

    VKRenderer::VKRenderer() = default;

    VKRenderer::~VKRenderer()
    {
        if (fDevice != VK_NULL_HANDLE)
        {
            RetireAcquiredFrame();
            vkDeviceWaitIdle(fDevice);

            fImageEntries.clear();

            if (fInFlightFence != VK_NULL_HANDLE)
                vkDestroyFence(fDevice, fInFlightFence, nullptr);
            DestroyRenderFinishedSemaphores();
            if (fImageAvailableSemaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(fDevice, fImageAvailableSemaphore, nullptr);

            if (fCommandBuffer != VK_NULL_HANDLE)
                vkFreeCommandBuffers(fDevice, fContext.GetCommandPool(), 1, &fCommandBuffer);

            for (const VkDescriptorPool descriptorPool : fDescriptorPools)
                vkDestroyDescriptorPool(fDevice, descriptorPool, nullptr);
        }
    }

    int VKRenderer::Init(const OIV_RendererInitializationParams& initParams)
    {
        constexpr int DefaultWidth  = 1200;
        constexpr int DefaultHeight = 800;

        VKContext::CreateParams contextParams{};
        contextParams.window           = initParams.container;
        contextParams.nativeDisplay    = initParams.nativeDisplay;
        contextParams.width            = fViewportSize[0] > 0.0F ? static_cast<int>(fViewportSize[0]) : DefaultWidth;
        contextParams.height           = fViewportSize[1] > 0.0F ? static_cast<int>(fViewportSize[1]) : DefaultHeight;
        contextParams.gpuIndex         = initParams.gpuIndex;
        contextParams.adapterName      = initParams.adapterName;

        fContext.Init(contextParams);

        fDevice          = fContext.GetDevice();
        fPhysicalDevice  = fContext.GetPhysicalDevice();
        fSwapChainExtent = fContext.GetSwapChainExtent();

        fDataPath     = initParams.dataPath;
        fViewportSize = {static_cast<float>(contextParams.width), static_cast<float>(contextParams.height)};
        if (fContext.GetSwapChain() != VK_NULL_HANDLE)
        {
            CreatePipelines();
            fSwapChainRecovery = SwapChainRecovery::Ready;
        }
        else
            fSwapChainRecovery = SwapChainRecovery::SwapChain;

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool        = fContext.GetCommandPool();
        allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkResult result = vkAllocateCommandBuffers(fDevice, &allocInfo, &fCommandBuffer);
        CheckVkResult(result, "Failed to allocate command buffer");

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkSemaphore imageAvailable;
        result = vkCreateSemaphore(fDevice, &semaphoreInfo, nullptr, &imageAvailable);
        CheckVkResult(result, "Failed to create image available semaphore");
        fImageAvailableSemaphore = imageAvailable;
        CreateRenderFinishedSemaphores();

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VkFence fence;
        result = vkCreateFence(fDevice, &fenceInfo, nullptr, &fence);
        CheckVkResult(result, "Failed to create fence");
        fInFlightFence = fence;

        return 0;
    }

    void VKRenderer::CreatePipelines()
    {
        const std::filesystem::path shaderCachePath     = std::filesystem::path(fDataPath) / "ShaderCache";
        const std::filesystem::path vertShader          = FindShaderFile("QuadVP.vert.spv", shaderCachePath);
        const std::filesystem::path fragShader          = FindShaderFile("QuadFP.frag.spv", shaderCachePath);
        const std::filesystem::path selectionFragShader = FindShaderFile("QuadSelectionFP.frag.spv", shaderCachePath);
        const std::filesystem::path simpleFragShader    = FindShaderFile("QuadSimpleFP.frag.spv", shaderCachePath);
        const std::vector<VkImageView> swapChainImageViews = fContext.GetSwapChainImageViews();

        auto imagePipeline = std::make_unique<VKPipeline>(
            VKPipeline::CreateInfo{fDevice, fSwapChainExtent, fContext.GetSwapChainFormat(), vertShader, fragShader,
                                   true, sizeof(PushConstants), swapChainImageViews});
        auto selectionPipeline = std::make_unique<VKPipeline>(VKPipeline::CreateInfo{fDevice,
                                                                                     fSwapChainExtent,
                                                                                     fContext.GetSwapChainFormat(),
                                                                                     vertShader,
                                                                                     selectionFragShader,
                                                                                     false,
                                                                                     sizeof(SelectionPushConstants),
                                                                                     {}});
        auto overlayPipeline   = std::make_unique<VKPipeline>(VKPipeline::CreateInfo{fDevice,
                                                                                     fSwapChainExtent,
                                                                                     fContext.GetSwapChainFormat(),
                                                                                     vertShader,
                                                                                     simpleFragShader,
                                                                                     true,
                                                                                     sizeof(PushConstants),
                                                                                     {}});

        fImagePipeline     = std::move(imagePipeline);
        fSelectionPipeline = std::move(selectionPipeline);
        fOverlayPipeline   = std::move(overlayPipeline);
        // Recreated layouts are structurally compatible with existing descriptor sets, but their samplers are new.
        for (auto& item : fImageEntries)
        {
            item.second.descriptorSampler = VK_NULL_HANDLE;
            // Renderables registered before a deferred initialization still need their first descriptor set.
            if (item.second.descriptorSet == VK_NULL_HANDLE)
                AllocateDescriptorSet(item.second);
        }
        fPipelineFormat = fContext.GetSwapChainFormat();
    }

    int VKRenderer::SetViewParams(const ViewParameters& viewParams)
    {
        std::lock_guard<std::mutex> lock(fMutex);
        UpdateViewportSize(static_cast<int>(viewParams.uViewportSize.x), static_cast<int>(viewParams.uViewportSize.y));
        fTransparencyColors[0] = ToFloatColor(viewParams.uTransparencyColor1);
        fTransparencyColors[1] = ToFloatColor(viewParams.uTransparencyColor2);
        fShowGrid              = viewParams.showGrid;
        return 0;
    }

    void VKRenderer::UpdateViewportSize(int width, int height)
    {
        const bool sizeChanged = fViewportSize[0] != static_cast<float>(width) ||
                                 fViewportSize[1] != static_cast<float>(height);
        fViewportSize          = {static_cast<float>(width), static_cast<float>(height)};
        if (sizeChanged && fSwapChainRecovery == SwapChainRecovery::Ready)
            fSwapChainRecovery = SwapChainRecovery::SwapChain;
    }

    bool VKRenderer::RecreateSwapChainResources()
    {
        if (fContext.GetSwapChain() != VK_NULL_HANDLE)
        {
            CheckVkResult(RetireAcquiredFrame(), "Failed to retire the acquired Vulkan frame");
            CheckVkResult(vkDeviceWaitIdle(fDevice), "Failed to wait for Vulkan swap-chain retirement");
            fFrameSubmitted = false;
            DestroyRenderFinishedSemaphores();
            if (fImagePipeline != nullptr)
                fImagePipeline->DestroyFramebuffers();
        }

        const bool created = fContext.RecreateSwapChain(static_cast<int>(fViewportSize[0]),
                                                        static_cast<int>(fViewportSize[1]),
                                                        fSwapChainRecovery == SwapChainRecovery::Surface);
        // A successful surface replacement need not be repeated if its extent is still zero.
        fSwapChainRecovery = SwapChainRecovery::SwapChain;
        if (!created)
            return false;

        fSwapChainExtent = fContext.GetSwapChainExtent();
        if (fContext.GetSwapChainFormat() == fPipelineFormat)
            fImagePipeline->RecreateFramebuffers(fDevice, fContext.GetSwapChainImageViews(), fSwapChainExtent);
        else
            CreatePipelines();
        CreateRenderFinishedSemaphores();
        fSwapChainRecovery = SwapChainRecovery::Ready;
        return true;
    }

    VkResult VKRenderer::RetireAcquiredFrame()
    {
        // Before destroying the swap chain, consume an acquire signal that a failed frame never submitted.
        // The caller waits for the device before destroying any resources.
        if (fAcquiredImageIndex.has_value() && !fFrameSubmitted)
        {
            const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            VkSubmitInfo submitInfo{};
            submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.waitSemaphoreCount = 1;
            submitInfo.pWaitSemaphores    = &fImageAvailableSemaphore;
            submitInfo.pWaitDstStageMask  = &waitStage;
            const VkResult result         = vkQueueSubmit(fContext.GetGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
            if (result != VK_SUCCESS)
                return result;
        }
        fAcquiredImageIndex.reset();
        return VK_SUCCESS;
    }

    int VKRenderer::Redraw()
    {
        std::lock_guard<std::mutex> lock(fMutex);

        // Rendering is event-driven: a recovered surface must not consume the only requested redraw.
        constexpr int MaxFrameAttempts = 2;
        for (int attempt = 0; attempt < MaxFrameAttempts; ++attempt)
        {
            if (RenderFrame())
                return 0;
        }
        throw std::runtime_error("The Vulkan surface kept changing during redraw");
    }

    bool VKRenderer::RenderFrame()
    {
        // Zero-sized windows defer the redraw until restoration, without consuming the recovery retry budget.
        if (fViewportSize[0] <= 0.0F || fViewportSize[1] <= 0.0F ||
            (fSwapChainRecovery != SwapChainRecovery::Ready && !RecreateSwapChainResources()))
            return true;

        VkDevice device = fContext.GetDevice();
        if (fFrameSubmitted)
        {
            CheckVkResult(vkWaitForFences(device, 1, &fInFlightFence, VK_TRUE, UINT64_MAX),
                          "Failed to wait for the Vulkan render fence");
            if (fAcquiredImageIndex.has_value())
            {
                // Presentation allocation failures leave the submitted frame available for another present.
                PresentImage();
            }
            fFrameSubmitted = false;
            if (fSwapChainRecovery != SwapChainRecovery::Ready)
                return false;
        }

        for (auto& item : fImageEntries)
        {
            ImageEntry& entry = item.second;
            if (entry.texture != nullptr)
                entry.texture->ReleaseUploadResources(fUploadBufferCache);
        }

        if (!fAcquiredImageIndex.has_value())
        {
            uint32_t imageIndex{0};
            const VkResult result = vkAcquireNextImageKHR(device, fContext.GetSwapChain(), UINT64_MAX,
                                                          fImageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

            if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_ERROR_SURFACE_LOST_KHR)
            {
                fSwapChainRecovery = result == VK_ERROR_SURFACE_LOST_KHR ? SwapChainRecovery::Surface
                                                                         : SwapChainRecovery::SwapChain;
                return false;
            }
            else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            {
                throw VKException(result, "Failed to acquire swap chain image");
            }
            fAcquiredImageIndex = imageIndex;
        }
        const uint32_t imageIndex = *fAcquiredImageIndex;

        CheckVkResult(vkResetCommandBuffer(fCommandBuffer, 0), "Failed to reset the Vulkan command buffer");

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        CheckVkResult(vkBeginCommandBuffer(fCommandBuffer, &beginInfo), "Failed to begin the Vulkan command buffer");

        std::vector<ImageEntry*> uploadedEntries;
        for (auto& item : fImageEntries)
        {
            ImageEntry& entry                      = item.second;
            const OIV_Image_Render_mode renderMode = entry.renderable->GetImageRenderMode();
            if ((renderMode == OIV_Image_Render_mode::IRM_MainImage ||
                 renderMode == OIV_Image_Render_mode::IRM_Overlay) &&
                PrepareImage(entry))
            {
                uploadedEntries.push_back(&entry);
            }
        }

        VkClearValue clearColor{};
        clearColor.color = {{45.0f / 255.0f, 45.0f / 255.0f, 48.0f / 255.0f, 1.0f}};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = fImagePipeline->GetRenderPass();
        renderPassInfo.framebuffer       = fImagePipeline->GetFramebuffers()[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = fSwapChainExtent;
        renderPassInfo.clearValueCount   = 1;
        renderPassInfo.pClearValues      = &clearColor;

        vkCmdBeginRenderPass(fCommandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x        = 0.0f;
        viewport.y        = 0.0f;
        viewport.width    = static_cast<float>(fSwapChainExtent.width);
        viewport.height   = static_cast<float>(fSwapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(fCommandBuffer, 0, 1, &viewport);

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = fSwapChainExtent;
        vkCmdSetScissor(fCommandBuffer, 0, 1, &scissor);

        for (auto& item : fImageEntries)
        {
            ImageEntry& entry = item.second;
            if (entry.renderable->GetImageRenderMode() == OIV_Image_Render_mode::IRM_MainImage)
                DrawImage(entry, *fImagePipeline, OIV_Image_Render_mode::IRM_MainImage);
        }

        DrawSelectionRect();

        for (auto& item : fImageEntries)
        {
            ImageEntry& entry = item.second;
            if (entry.renderable->GetImageRenderMode() == OIV_Image_Render_mode::IRM_Overlay)
                DrawImage(entry, *fOverlayPipeline, OIV_Image_Render_mode::IRM_Overlay);
        }

        vkCmdEndRenderPass(fCommandBuffer);
        CheckVkResult(vkEndCommandBuffer(fCommandBuffer), "Failed to finish the Vulkan command buffer");

        VkPipelineStageFlags waitStages[]         = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        const VkSemaphore renderFinishedSemaphore = fRenderFinishedSemaphores[imageIndex];

        VkSubmitInfo submitInfo{};
        submitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount   = 1;
        submitInfo.pWaitSemaphores      = &fImageAvailableSemaphore;
        submitInfo.pWaitDstStageMask    = waitStages;
        submitInfo.commandBufferCount   = 1;
        submitInfo.pCommandBuffers      = &fCommandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores    = &renderFinishedSemaphore;

        CheckVkResult(vkResetFences(device, 1, &fInFlightFence), "Failed to reset the Vulkan render fence");
        const VkResult result = vkQueueSubmit(fContext.GetGraphicsQueue(), 1, &submitInfo, fInFlightFence);
        CheckVkResult(result, "Failed to submit draw command buffer");
        fFrameSubmitted = true;

        for (ImageEntry* entry : uploadedEntries)
            entry->renderable->ClearImageDirty();

        return PresentImage();
    }

    bool VKRenderer::PresentImage()
    {
        const uint32_t imageIndex                 = *fAcquiredImageIndex;
        const VkSemaphore renderFinishedSemaphore = fRenderFinishedSemaphores[imageIndex];
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores    = &renderFinishedSemaphore;

        VkSwapchainKHR swapChain   = fContext.GetSwapChain();
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains    = &swapChain;
        presentInfo.pImageIndices  = &imageIndex;

        const VkResult result = vkQueuePresentKHR(fContext.GetPresentQueue(), &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_SURFACE_LOST_KHR)
        {
            fAcquiredImageIndex.reset();
            fSwapChainRecovery = result == VK_ERROR_SURFACE_LOST_KHR ? SwapChainRecovery::Surface
                                                                     : SwapChainRecovery::SwapChain;
        }
        else
        {
            CheckVkResult(result, "Failed to present the Vulkan swap chain");
            fAcquiredImageIndex.reset();
        }
        return result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR;
    }

    int VKRenderer::SetFilterLevel(OIV_Filter_type filterType)
    {
        std::lock_guard<std::mutex> lock(fMutex);
        fFilterType = filterType;
        return 0;
    }

    int VKRenderer::SetSelectionRect(VisualSelectionRect selectionRect)
    {
        std::lock_guard<std::mutex> lock(fMutex);
        fSelectionRect = selectionRect;
        return 0;
    }

    int VKRenderer::SetExposure(const OIV_CMD_ColorExposure_Request& exposure)
    {
        std::lock_guard<std::mutex> lock(fMutex);
        fExposure   = static_cast<float>(exposure.exposure);
        fOffset     = static_cast<float>(exposure.offset);
        fGamma      = static_cast<float>(exposure.gamma);
        fSaturation = static_cast<float>(exposure.saturation);
        return 0;
    }

    int VKRenderer::SetBackgroundColor(int index, LLUtils::Color backgroundColor)
    {
        std::lock_guard<std::mutex> lock(fMutex);
        if (index >= 0 && index < 2)
        {
            fBackgroundColors[index] = ToFloatColor(backgroundColor);
        }
        return 0;
    }

    int VKRenderer::AddRenderable(IRenderable* renderable)
    {
        if (renderable == nullptr)
            throw std::invalid_argument("Cannot add a null renderable");

        std::lock_guard<std::mutex> lock(fMutex);

        const auto [added, inserted] = fImageEntries.try_emplace(renderable->GetID(),
                                                                 ImageEntry{.renderable = renderable});
        if (!inserted)
            throw std::runtime_error("Cannot add the same renderable twice");
        try
        {
            if (fImagePipeline != nullptr)
                AllocateDescriptorSet(added->second);
        }
        catch (...)
        {
            fImageEntries.erase(added);
            throw;
        }
        return 0;
    }

    int VKRenderer::RemoveRenderable(IRenderable* renderable)
    {
        if (renderable == nullptr)
            throw std::invalid_argument("Cannot remove a null renderable");

        std::lock_guard<std::mutex> lock(fMutex);

        const auto entry = fImageEntries.find(renderable->GetID());
        if (entry == fImageEntries.end())
            throw std::runtime_error("Cannot remove an unknown renderable");

        if (fDevice != VK_NULL_HANDLE)
        {
            if (fFrameSubmitted)
            {
                const VkResult result = vkWaitForFences(fDevice, 1, &fInFlightFence, VK_TRUE, UINT64_MAX);
                // Device loss counts as completion for resource lifetimes; image destructors must still clean up.
                if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST)
                    CheckVkResult(result, "Failed to wait for Vulkan rendering before removing an image");
            }
            if (entry->second.texture != nullptr)
                entry->second.texture->ReleaseUploadResources(fUploadBufferCache);
            if (entry->second.descriptorSet != VK_NULL_HANDLE)
            {
                CheckVkResult(vkFreeDescriptorSets(fDevice, entry->second.descriptorPool, 1,
                                                   &entry->second.descriptorSet),
                              "Failed to release a Vulkan descriptor set");
            }
        }
        fImageEntries.erase(entry);
        return 0;
    }

    VkDescriptorPool VKRenderer::CreateDescriptorPool()
    {
        constexpr uint32_t DescriptorCount = 32;

        VkDescriptorPoolSize poolSize{};
        poolSize.type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = DescriptorCount;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes    = &poolSize;
        poolInfo.maxSets       = DescriptorCount;

        VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
        VkResult result = vkCreateDescriptorPool(fDevice, &poolInfo, nullptr, &descriptorPool);
        CheckVkResult(result, "Failed to create descriptor pool");
        try
        {
            fDescriptorPools.push_back(descriptorPool);
        }
        catch (...)
        {
            vkDestroyDescriptorPool(fDevice, descriptorPool, nullptr);
            throw;
        }
        return descriptorPool;
    }

    void VKRenderer::AllocateDescriptorSet(ImageEntry& entry)
    {
        VkDescriptorSetLayout layout = fImagePipeline->GetDescriptorSetLayout();
        if (layout == VK_NULL_HANDLE)
            return;

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts        = &layout;

        VkDescriptorPool descriptorPool = fDescriptorPools.empty() ? CreateDescriptorPool() : fDescriptorPools.back();
        allocInfo.descriptorPool        = descriptorPool;
        VkResult result                 = vkAllocateDescriptorSets(fDevice, &allocInfo, &entry.descriptorSet);
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
        {
            // Removed renderables may have freed capacity in an older pool. Keep the usual path to one allocation.
            for (size_t i = 0; i + 1 < fDescriptorPools.size() &&
                               (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL);
                 ++i)
            {
                descriptorPool           = fDescriptorPools[i];
                allocInfo.descriptorPool = descriptorPool;
                result                   = vkAllocateDescriptorSets(fDevice, &allocInfo, &entry.descriptorSet);
                if (result == VK_SUCCESS)
                    std::swap(fDescriptorPools[i], fDescriptorPools.back());
            }
            if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL)
            {
                descriptorPool           = CreateDescriptorPool();
                allocInfo.descriptorPool = descriptorPool;
                result                   = vkAllocateDescriptorSets(fDevice, &allocInfo, &entry.descriptorSet);
            }
        }
        CheckVkResult(result, "Failed to allocate descriptor set");
        entry.descriptorPool = descriptorPool;
    }

    void VKRenderer::CreateRenderFinishedSemaphores()
    {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        std::vector<VkSemaphore> semaphores(fContext.GetSwapChainImages().size(), VK_NULL_HANDLE);
        try
        {
            for (VkSemaphore& semaphore : semaphores)
            {
                VkSemaphore created;
                CheckVkResult(vkCreateSemaphore(fDevice, &semaphoreInfo, nullptr, &created),
                              "Failed to create render finished semaphore");
                semaphore = created;
            }
        }
        catch (...)
        {
            for (const VkSemaphore semaphore : semaphores)
            {
                if (semaphore != VK_NULL_HANDLE)
                    vkDestroySemaphore(fDevice, semaphore, nullptr);
            }
            throw;
        }
        fRenderFinishedSemaphores = std::move(semaphores);
    }

    void VKRenderer::DestroyRenderFinishedSemaphores()
    {
        for (const VkSemaphore semaphore : fRenderFinishedSemaphores)
            vkDestroySemaphore(fDevice, semaphore, nullptr);
        fRenderFinishedSemaphores.clear();
    }

    bool VKRenderer::PrepareImage(ImageEntry& entry)
    {
        IRenderable& renderable = *entry.renderable;

        if (!renderable.GetVisible() || renderable.GetOpacity() <= 0.0)
            return false;

        renderable.PreRender();
        if (renderable.GetIsImageDirty() || entry.texture == nullptr)
        {
            IMCodec::ImageSharedPtr image = renderable.GetImage();
            if (image == nullptr)
                return false;

            if (image->GetTexelFormat() != IMCodec::TexelFormat::I_R8_G8_B8_A8)
                image = IMUtil::ImageUtil::Convert(image, IMCodec::TexelFormat::I_R8_G8_B8_A8);
            if (image == nullptr)
                throw std::runtime_error("Could not convert image for Vulkan rendering");

            if (entry.texture == nullptr || entry.texture->GetWidth() != image->GetWidth() ||
                entry.texture->GetHeight() != image->GetHeight())
            {
                entry.texture             = std::make_unique<VKTexture>(fDevice, fPhysicalDevice, image->GetWidth(),
                                                                        image->GetHeight(), VK_FORMAT_R8G8B8A8_UNORM);
                entry.descriptorImageView = VK_NULL_HANDLE;
            }

            entry.texture->Upload(fCommandBuffer, image->GetBuffer(), image->GetRowPitchInBytes(), fUploadBufferCache);
            return true;
        }

        return false;
    }

    void VKRenderer::DrawImage(ImageEntry& entry, VKPipeline& pipeline, OIV_Image_Render_mode renderMode)
    {
        IRenderable& renderable = *entry.renderable;
        const double opacity    = renderable.GetOpacity();
        if (!renderable.GetVisible() || opacity <= 0.0)
            return;

        if (entry.texture == nullptr || entry.descriptorSet == VK_NULL_HANDLE)
            return;

        PushConstants pushConstants{};
        std::memcpy(pushConstants.viewportSize, fViewportSize.data(), sizeof(pushConstants.viewportSize));
        const LLUtils::PointF64 imageScale    = renderable.GetScale();
        const LLUtils::PointF64 imagePosition = renderable.GetPosition();

        if (renderMode == OIV_Image_Render_mode::IRM_Overlay)
        {
            const double oppositeX = imagePosition.x + static_cast<double>(entry.texture->GetWidth()) * imageScale.x;
            const double oppositeY = imagePosition.y + static_cast<double>(entry.texture->GetHeight()) * imageScale.y;
            const double framebufferScaleX = static_cast<double>(fSwapChainExtent.width) / fViewportSize[0];
            const double framebufferScaleY = static_cast<double>(fSwapChainExtent.height) / fViewportSize[1];
            const double left   = std::clamp(std::floor(std::min(imagePosition.x, oppositeX) * framebufferScaleX), 0.0,
                                             static_cast<double>(fSwapChainExtent.width));
            const double top    = std::clamp(std::floor(std::min(imagePosition.y, oppositeY) * framebufferScaleY), 0.0,
                                             static_cast<double>(fSwapChainExtent.height));
            const double right  = std::clamp(std::ceil(std::max(imagePosition.x, oppositeX) * framebufferScaleX), 0.0,
                                             static_cast<double>(fSwapChainExtent.width));
            const double bottom = std::clamp(std::ceil(std::max(imagePosition.y, oppositeY) * framebufferScaleY), 0.0,
                                             static_cast<double>(fSwapChainExtent.height));
            if (right <= left || bottom <= top)
                return;

            VkRect2D scissor{};
            scissor.offset = {static_cast<int32_t>(left), static_cast<int32_t>(top)};
            scissor.extent = {static_cast<uint32_t>(right - left), static_cast<uint32_t>(bottom - top)};
            vkCmdSetScissor(fCommandBuffer, 0, 1, &scissor);
        }

        pushConstants.imageSize[0]   = static_cast<float>(entry.texture->GetWidth());
        pushConstants.imageSize[1]   = static_cast<float>(entry.texture->GetHeight());
        pushConstants.imageScale[0]  = static_cast<float>(imageScale.x);
        pushConstants.imageScale[1]  = static_cast<float>(imageScale.y);
        pushConstants.imageOffset[0] = static_cast<float>(imagePosition.x);
        pushConstants.imageOffset[1] = static_cast<float>(imagePosition.y);
        std::memcpy(pushConstants.backgroundColor1, fBackgroundColors[0].data(),
                    sizeof(pushConstants.backgroundColor1));
        std::memcpy(pushConstants.backgroundColor2, fBackgroundColors[1].data(),
                    sizeof(pushConstants.backgroundColor2));
        std::memcpy(pushConstants.transparencyColor1, fTransparencyColors[0].data(),
                    sizeof(pushConstants.transparencyColor1));
        std::memcpy(pushConstants.transparencyColor2, fTransparencyColors[1].data(),
                    sizeof(pushConstants.transparencyColor2));
        pushConstants.opacity         = static_cast<float>(opacity);
        pushConstants.exposure        = fExposure;
        pushConstants.colorOffset     = fOffset;
        pushConstants.gamma           = fGamma;
        pushConstants.saturation      = fSaturation;
        pushConstants.showGrid        = fShowGrid ? 1 : 0;

        const OIV_Filter_type requestedFilter = renderable.GetFilterType();
        const OIV_Filter_type filter          = requestedFilter == FT_Lanczos3 ? fFilterType : requestedFilter;
        const VkFilter vkFilter               = filter == FT_None ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
        const VkImageView imageView           = entry.texture->GetImageView();
        const VkSampler sampler               = pipeline.GetSampler(vkFilter);
        if (entry.descriptorImageView != imageView || entry.descriptorSampler != sampler)
        {
            pipeline.UpdateDescriptorSet(fDevice, entry.descriptorSet, imageView, sampler);
            entry.descriptorImageView = imageView;
            entry.descriptorSampler   = sampler;
        }

        vkCmdBindPipeline(fCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipeline());
        vkCmdBindDescriptorSets(fCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.GetPipelineLayout(), 0, 1,
                                &entry.descriptorSet, 0, nullptr);
        vkCmdPushConstants(fCommandBuffer, pipeline.GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(PushConstants), &pushConstants);
        vkCmdDraw(fCommandBuffer, 4, 1, 0, 0);
    }

    void VKRenderer::DrawSelectionRect()
    {
        if (fSelectionRect.p0.x == -1)
            return;

        SelectionPushConstants pushConstants{};
        std::memcpy(pushConstants.viewportSize, fViewportSize.data(), sizeof(pushConstants.viewportSize));
        pushConstants.selectionRect[0] = static_cast<float>(fSelectionRect.p0.x);
        pushConstants.selectionRect[1] = static_cast<float>(fSelectionRect.p0.y);
        pushConstants.selectionRect[2] = static_cast<float>(fSelectionRect.p1.x);
        pushConstants.selectionRect[3] = static_cast<float>(fSelectionRect.p1.y);

        vkCmdBindPipeline(fCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fSelectionPipeline->GetPipeline());
        vkCmdPushConstants(fCommandBuffer, fSelectionPipeline->GetPipelineLayout(), VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                           sizeof(SelectionPushConstants), &pushConstants);
        vkCmdDraw(fCommandBuffer, 4, 1, 0, 0);
    }
    const char* VKRenderer::GetGPUName() const
    {
        if (fGPUName.empty())
        {
            fGPUName = fContext.GetPhysicalDeviceProperties().deviceName;
            if (fGPUName.empty())
                fGPUName = "Unknown";
        }
        return fGPUName.c_str();
    }

    const char* VKRenderer::GetAPIVersion() const
    {
        if (fAPIVersion.empty())
        {
            const uint32_t apiVersion = fContext.GetPhysicalDeviceProperties().apiVersion;
            fAPIVersion               = std::to_string(VK_API_VERSION_MAJOR(apiVersion)) + "." +
                                        std::to_string(VK_API_VERSION_MINOR(apiVersion));
        }
        return fAPIVersion.c_str();
    }

    const char* VKRenderer::GetDriverVersion() const
    {
        if (fDriverVersion.empty())
            fDriverVersion = std::to_string(fContext.GetPhysicalDeviceProperties().driverVersion);
        return fDriverVersion.c_str();
    }
}  // namespace OIV
