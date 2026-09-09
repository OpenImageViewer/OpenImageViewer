#pragma once
#include <cstddef>
#include <type_traits>

#include "VKContext.h"
#include "VKPipeline.h"
#include "VKTexture.h"
#include <Defs.h>
#include <Interfaces/IRenderer.h>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace OIV
{
    class VKRenderer final : public IRenderer
    {
      public:

        VKRenderer();
        ~VKRenderer() override;

        std::vector<RendererAdapter> EnumerateAdapters() override { return VKContext::EnumerateAdapters(); }
        Acceleration GetAcceleration() const override { return fContext.GetAcceleration(); }
        int Init(const OIV_RendererInitializationParams& initParams) override;
        int SetViewParams(const ViewParameters& viewParams) override;
        int Redraw() override;
        int SetFilterLevel(OIV_Filter_type filterType) override;
        int SetSelectionRect(VisualSelectionRect selectionRect) override;
        int SetExposure(const OIV_CMD_ColorExposure_Request& exposure) override;
        int AddRenderable(IRenderable* renderable) override;
        int RemoveRenderable(IRenderable* renderable) override;
        int SetBackgroundColor(int index, LLUtils::Color backgroundColor) override;

        const char* GetBackendName() const override { return "Vulkan"; }
        const char* GetGPUName() const override;
        const char* GetAPIVersion() const override;
        const char* GetDriverVersion() const override;
        int GetSelectedGPUIndex() const override { return fContext.GetGpuIndex(); }

      private:

        struct ImageEntry
        {
            IRenderable* renderable{};
            std::unique_ptr<VKTexture> texture;
            VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
            VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
            VkImageView descriptorImageView{VK_NULL_HANDLE};
            VkSampler descriptorSampler{VK_NULL_HANDLE};
        };

        struct PushConstants
        {
            alignas(8) float viewportSize[2];
            alignas(8) float imageSize[2];
            alignas(8) float imageScale[2];
            alignas(8) float imageOffset[2];
            alignas(16) float backgroundColor1[4];
            alignas(16) float backgroundColor2[4];
            alignas(16) float transparencyColor1[4];
            alignas(16) float transparencyColor2[4];
            float opacity;
            float exposure;
            float colorOffset;
            float gamma;
            float saturation;
            int32_t showGrid;
        };

        struct SelectionPushConstants
        {
            alignas(8) float viewportSize[2];
            alignas(16) float selectionRect[4];
        };

        // Mirrors VKImage.shader and QuadSelectionFP.shader's push-constant layout.
        static_assert(std::is_standard_layout_v<PushConstants> && std::is_standard_layout_v<SelectionPushConstants>);
        static_assert(alignof(PushConstants) == 16 && sizeof(PushConstants) == 128);
        static_assert(offsetof(PushConstants, viewportSize) == 0 && offsetof(PushConstants, imageSize) == 8);
        static_assert(offsetof(PushConstants, imageScale) == 16 && offsetof(PushConstants, imageOffset) == 24);
        static_assert(offsetof(PushConstants, backgroundColor1) == 32 &&
                      offsetof(PushConstants, backgroundColor2) == 48);
        static_assert(offsetof(PushConstants, transparencyColor1) == 64 &&
                      offsetof(PushConstants, transparencyColor2) == 80);
        static_assert(offsetof(PushConstants, opacity) == 96 && offsetof(PushConstants, exposure) == 100);
        static_assert(offsetof(PushConstants, colorOffset) == 104 && offsetof(PushConstants, gamma) == 108);
        static_assert(offsetof(PushConstants, saturation) == 112 && offsetof(PushConstants, showGrid) == 116);
        static_assert(alignof(SelectionPushConstants) == 16 && sizeof(SelectionPushConstants) == 32);
        static_assert(offsetof(SelectionPushConstants, viewportSize) == 0 &&
                      offsetof(SelectionPushConstants, selectionRect) == 16);

        VkDescriptorPool CreateDescriptorPool();
        void AllocateDescriptorSet(ImageEntry& entry);
        void CreatePipelines();
        void CreateRenderFinishedSemaphores();
        void DestroyRenderFinishedSemaphores();
        bool RecreateSwapChainResources();
        VkResult RetireAcquiredFrame();
        bool RenderFrame();
        bool PresentImage();
        bool PrepareImage(ImageEntry& entry);
        void DrawImage(ImageEntry& entry, VKPipeline& pipeline, OIV_Image_Render_mode renderMode);
        void DrawSelectionRect();
        void UpdateViewportSize(int width, int height);

        enum class SwapChainRecovery
        {
            Ready,
            SwapChain,
            Surface
        };

        VKContext fContext;
        VkDevice fDevice{VK_NULL_HANDLE};
        VkPhysicalDevice fPhysicalDevice{VK_NULL_HANDLE};
        VkExtent2D fSwapChainExtent{0, 0};
        VkFormat fPipelineFormat{VK_FORMAT_UNDEFINED};
        SwapChainRecovery fSwapChainRecovery{SwapChainRecovery::Ready};

        std::vector<VkDescriptorPool> fDescriptorPools;

        VKPipelineUniquePtr fImagePipeline;
        VKPipelineUniquePtr fSelectionPipeline;
        VKPipelineUniquePtr fOverlayPipeline;

        VkCommandBuffer fCommandBuffer{VK_NULL_HANDLE};
        VkSemaphore fImageAvailableSemaphore{VK_NULL_HANDLE};
        // Reacquiring a swap-chain image proves presentation consumed that image's previous semaphore signal.
        std::vector<VkSemaphore> fRenderFinishedSemaphores;
        VkFence fInFlightFence{VK_NULL_HANDLE};
        // Keep an acquired image across recording/submission failures so its semaphore is consumed exactly once.
        std::optional<uint32_t> fAcquiredImageIndex;
        bool fFrameSubmitted{};

        std::unique_ptr<VKUploadBuffer> fUploadBufferCache;
        std::map<uint32_t, ImageEntry> fImageEntries;
        mutable std::mutex fMutex;
        std::array<float, 2> fViewportSize{};
        std::array<std::array<float, 4>, 2> fBackgroundColors = {
            {std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F}, std::array<float, 4>{0.0F, 0.0F, 0.16F, 1.0F}}};
        std::array<std::array<float, 4>, 2> fTransparencyColors = {
            {std::array<float, 4>{0.75F, 0.75F, 0.75F, 1.0F}, std::array<float, 4>{1.0F, 1.0F, 1.0F, 1.0F}}};
        VisualSelectionRect fSelectionRect{{-1, -1}, {-1, -1}};
        OIV_Filter_type fFilterType{FT_Linear};
        float fExposure{1.0F};
        float fOffset{};
        float fGamma{1.0F};
        float fSaturation{1.0F};
        bool fShowGrid{};
        OIVString fDataPath;
        mutable std::string fGPUName;
        mutable std::string fAPIVersion;
        mutable std::string fDriverVersion;
    };
}  // namespace OIV
