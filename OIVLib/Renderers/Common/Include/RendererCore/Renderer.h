#pragma once
#include <RendererCore/IRenderBackend.h>
#include <memory>

namespace OIV
{
    class Renderer;

    // Move-only owner of one registration. All operations run on the owner thread,
    // and its Renderer must remain alive until this owner is destroyed.
    class RenderImage
    {
      public:

        ~RenderImage();
        RenderImage(const RenderImage&)            = delete;
        RenderImage& operator=(const RenderImage&) = delete;
        RenderImage(RenderImage&& other) noexcept;
        RenderImage& operator=(RenderImage&& other) noexcept;
        void SetImage(const IMCodec::ImageSharedPtr& pixels);
        void SetProperties(const RenderImageProperties& properties);

      private:

        friend class Renderer;
        RenderImage(Renderer& owner, uint64_t id) noexcept;
        // Non-owning stable facade, null only after a move.
        Renderer* fOwner;
        // Renderer-local identity distinct from C ImageHandle, never recycled.
        uint64_t fId;
    };

    // Common presentation owner around one initialized selected graphics backend.
    // Nonmovability keeps every RenderImage's borrowed owner address stable.
    // Presentation mutation stays on the owner thread and cannot reenter during Render.
    class Renderer
    {
      public:

        explicit Renderer(RenderBackendSharedPtr backend);
        ~Renderer();
        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&)                 = delete;
        Renderer& operator=(Renderer&&)      = delete;
        RenderImage CreateImage(IMCodec::ImageSharedPtr pixels, const RenderImageProperties& properties = {});
        int Redraw();
        void Shutdown() noexcept;
        int SetViewParams(const ViewParameters& params);
        int SetFilterLevel(OIV_Filter_type filter);
        int SetExposure(const OIV_CMD_ColorExposure_Request& exposure);
        int SetSelectionRect(VisualSelectionRect selection);
        int SetBackgroundColor(int index, LLUtils::Color color);
        Acceleration GetAcceleration() const;
        const char* GetBackendName() const;
        const char* GetGPUName() const;
        const char* GetAPIVersion() const;
        const char* GetDriverVersion() const;
        int GetSelectedGPUIndex() const;

      private:

        friend class RenderImage;
        void Release(uint64_t id) noexcept;
        void SetImage(uint64_t id, const IMCodec::ImageSharedPtr& pixels);
        void SetProperties(uint64_t id, const RenderImageProperties& properties);
        // Backend owns GPU resources and acknowledges its own successful uploads.
        RenderBackendSharedPtr fBackend;
        // Current frame view parameters supplied by API composition.
        ViewParameters fViewParams{};
        // Sole CPU record store, traversed directly during frames in creation order.
        std::map<uint64_t, RenderImageRecord> fImages;
        // Next identity, monotonically advanced only after successful insertion.
        uint64_t fNextId = 1;
        // Debug contract excludes reentrant mutation while records are borrowed.
#ifndef NDEBUG
        bool fRendering = false;
#endif
    };
}  // namespace OIV
