#pragma once
#include <cstddef>
#include <type_traits>

#include "VKContext.h"
#include "VKPipeline.h"
#include "VKTexture.h"
#include <RendererCore/RenderTypes.h>
#include <RendererCore/IRenderBackend.h>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace OIV
{
    // Vulkan graphics backend owning uploads, descriptors and submitted-work lifetimes.
    class VKRenderer final : public IRenderBackend
    {
      public:

        VKRenderer() = default;
        ~VKRenderer() override;

        std::vector<RendererAdapter> EnumerateAdapters() override { return VKContext::EnumerateAdapters(); }
        const RendererAdapter& GetSelectedAdapter() const override { return fContext.GetSelectedAdapter(); }
        void Init(const OIV::RendererInitialization& initParams) override;
        void SetViewParams(const ViewParameters& viewParams) override;
        void Render(const RenderFrame& frame) override;
        void SetFilterLevel(OIV::FilterType filterType) override;
        void SetSelectionRect(VisualSelectionRect selectionRect) override;
        void SetExposure(const OIV::ColorExposure& exposure) override;
        void ReleaseImageResources(uint64_t id) noexcept override;
        void SetBackgroundColor(int index, LLUtils::Color backgroundColor) override;

      private:

        // GPU cache keyed by renderer-local ID, with no borrowed record pointers.
        struct ImageEntry
        {
            // Last content revision successfully uploaded by this backend.
            uint64_t uploadedRevision = 0;
            // Revision recorded in this command buffer, acknowledged only on submission.
            uint64_t pendingRevision = 0;
            // Logical release completed, but GPU ownership still awaits safe retirement.
            bool retired = false;
            // Owned lazy GPU texture, absent until a visible nonempty record uploads.
            std::unique_ptr<VKTexture> texture;
            // Pool owning this descriptor set, retained until safe resource retirement.
            VkDescriptorPool descriptorPool{VK_NULL_HANDLE};
            // Lazy image descriptor, absent until a visible image needs resources.
            VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
            // Last image view written into the descriptor.
            VkImageView descriptorImageView{VK_NULL_HANDLE};
            // Last sampler written into the descriptor.
            VkSampler descriptorSampler{VK_NULL_HANDLE};
        };

        // Shader ABI data with layout checked or preserved by the existing backend.
        struct PushConstants
        {
            // Client viewport extent in pixels.
            alignas(8) float viewportSize[2];
            // Texture extent in texels.
            alignas(8) float imageSize[2];
            // Client pixels per texture texel along each axis.
            alignas(8) float imageScale[2];
            // Client placement in pixels.
            alignas(8) float imageOffset[2];
            // First RGBA background color for the existing background pattern.
            alignas(16) float backgroundColor1[4];
            // Second RGBA background color for the existing background pattern.
            alignas(16) float backgroundColor2[4];
            // First RGBA transparency-checker color.
            alignas(16) float transparencyColor1[4];
            // Second RGBA transparency-checker color.
            alignas(16) float transparencyColor2[4];
            // Presentation opacity multiplier.
            float opacity;
            // Existing color exposure multiplier.
            float exposure;
            // Existing color offset applied by the shader.
            float colorOffset;
            // Existing gamma correction value.
            float gamma;
            // Existing color saturation multiplier.
            float saturation;
            // Whether the texel grid is drawn.
            int32_t showGrid;
        };

        // Shader ABI data with layout checked or preserved by the existing backend.
        struct SelectionPushConstants
        {
            // Client viewport extent in pixels.
            alignas(8) float viewportSize[2];
            // Selection endpoints in client pixels.
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
        bool RenderFrame(const OIV::RenderFrame& frame);
        bool PresentImage();
        bool PrepareImage(ImageEntry& entry, const RenderImageRecord& record);
        void DrawImage(ImageEntry& entry, const RenderImageRecord& record, VKPipeline& pipeline,
                       OIV::ImageRenderMode renderMode);
        void DrawSelectionRect();
        void UpdateViewportSize(int width, int height);

        enum class SwapChainRecovery
        {
            Ready,
            SwapChain,
            Surface
        };

        // Owned graphics context, released before the native canvas is destroyed.
        VKContext fContext;
        // Current swap-chain dimensions in pixels.
        VkExtent2D fSwapChainExtent{0, 0};
        // Swap-chain format used by the current rendering pipelines.
        VkFormat fPipelineFormat{VK_FORMAT_UNDEFINED};
        // Pending surface/swap-chain recovery state for the existing bounded retry policy.
        SwapChainRecovery fSwapChainRecovery{SwapChainRecovery::Ready};

        // Owned descriptor pools reused across lazy image allocations.
        std::vector<VkDescriptorPool> fDescriptorPools;

        // Owned main-image pipeline and swap-chain rendering resources.
        VKPipelineUniquePtr fImagePipeline;
        // Owned selection overlay pipeline.
        VKPipelineUniquePtr fSelectionPipeline;
        // Owned text/overlay pipeline.
        VKPipelineUniquePtr fOverlayPipeline;

        // Reusable graphics command buffer whose borrowed record inputs end before submission.
        VkCommandBuffer fCommandBuffer{VK_NULL_HANDLE};
        // Acquire semaphore consumed once per acquired swap-chain image.
        VkSemaphore fImageAvailableSemaphore{VK_NULL_HANDLE};
        // Reacquiring a swap-chain image proves presentation consumed that image's previous semaphore signal.
        std::vector<VkSemaphore> fRenderFinishedSemaphores;
        // Fence protecting submitted image resources and upload storage.
        VkFence fInFlightFence{VK_NULL_HANDLE};
        // Keep an acquired image across recording/submission failures so its semaphore is consumed exactly once.
        std::optional<uint32_t> fAcquiredImageIndex;
        // Whether the previous command buffer requires completion before updating resources.
        bool fFrameSubmitted{};

        // Reusable completed upload buffer, independent of common record lifetime.
        std::unique_ptr<VKUploadBuffer> fUploadBufferCache;
        // Renderer-local GPU cache with successful upload revisions and retirement ownership.
        std::map<uint64_t, ImageEntry> fImageEntries;
        // Existing backend synchronization retained across the contract migration.
        mutable std::mutex fMutex;
        // Client viewport dimensions in pixels.
        std::array<float, 2> fViewportSize{};
        std::array<std::array<float, 4>, 2> fBackgroundColors = {
            {std::array<float, 4>{0.0F, 0.0F, 0.0F, 1.0F}, std::array<float, 4>{0.0F, 0.0F, 0.16F, 1.0F}}};
        std::array<std::array<float, 4>, 2> fTransparencyColors = {
            {std::array<float, 4>{0.75F, 0.75F, 0.75F, 1.0F}, std::array<float, 4>{1.0F, 1.0F, 1.0F, 1.0F}}};
        // Current client selection rectangle, with the existing absent sentinel.
        VisualSelectionRect fSelectionRect{{-1, -1}, {-1, -1}};
        // Global fallback sampling policy for the existing per-image filter behavior.
        OIV::FilterType fFilterType{OIV::FilterType::Linear};
        // Existing shader exposure multiplier.
        float fExposure{1.0F};
        // Existing shader color offset.
        float fOffset{};
        // Existing shader gamma value.
        float fGamma{1.0F};
        // Existing shader saturation multiplier.
        float fSaturation{1.0F};
        // Whether the existing texel-grid visualization is enabled.
        bool fShowGrid{};
    };
}  // namespace OIV
