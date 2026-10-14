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
        // Renderer-local identity, never recycled.
        uint64_t fId;
    };

    // Common presentation owner around one initialized selected graphics backend.
    // Nonmovability keeps every RenderImage's borrowed owner address stable.
    // Presentation mutation stays on the owner thread and cannot reenter during Render.
    class Renderer
    {
      public:

        explicit Renderer(std::unique_ptr<OIV::IRenderBackend> backend);
        ~Renderer();
        Renderer(const Renderer&)            = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&)                 = delete;
        Renderer& operator=(Renderer&&)      = delete;
        RenderImage CreateImage(IMCodec::ImageSharedPtr pixels, const RenderImageProperties& properties = {});
        void Redraw();
        void Shutdown() noexcept;
        void SetViewParams(const ViewParameters& params);
        void SetFilterLevel(OIV::FilterType filter);
        void SetExposure(const OIV::ColorExposure& exposure);
        void SetSelectionRect(VisualSelectionRect selection);
        void SetBackgroundColor(int index, LLUtils::Color color);
        const RendererAdapter& GetSelectedAdapter() const;

      private:

        friend class RenderImage;
        void Release(uint64_t id) noexcept;
        void SetImage(uint64_t id, const IMCodec::ImageSharedPtr& pixels);
        void SetProperties(uint64_t id, const RenderImageProperties& properties);
        // Backend owns GPU resources and acknowledges its own successful uploads.
        std::unique_ptr<OIV::IRenderBackend> fBackend;
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
