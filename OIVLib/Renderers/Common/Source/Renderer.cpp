#include <RendererCore/Renderer.h>
#include <LLUtils/Exception.h>
#include <cassert>
#include <utility>
#include <limits>

namespace OIV
{
    RenderImage::RenderImage(Renderer& owner, uint64_t id) noexcept : fOwner(&owner), fId(id) {}
    RenderImage::~RenderImage()
    {
        if (fOwner)
            fOwner->Release(fId);
    }
    RenderImage::RenderImage(RenderImage&& other) noexcept
        : fOwner(std::exchange(other.fOwner, nullptr)), fId(other.fId)
    {
    }
    RenderImage& RenderImage::operator=(RenderImage&& other) noexcept
    {
        if (this != &other)
        {
            if (fOwner)
                fOwner->Release(fId);
            fOwner = std::exchange(other.fOwner, nullptr);
            fId    = other.fId;
        }
        return *this;
    }
    void RenderImage::SetImage(const IMCodec::ImageSharedPtr& pixels)
    {
        assert(fOwner);
        fOwner->SetImage(fId, pixels);
    }
    void RenderImage::SetProperties(const RenderImageProperties& properties)
    {
        assert(fOwner);
        fOwner->SetProperties(fId, properties);
    }
    Renderer::Renderer(std::unique_ptr<OIV::IRenderBackend> backend) : fBackend(std::move(backend))
    {
        assert(fBackend);
    }
    Renderer::~Renderer()
    {
        assert(fImages.empty());
    }
    RenderImage Renderer::CreateImage(IMCodec::ImageSharedPtr pixels, const RenderImageProperties& properties)
    {
        assert(!fRendering);
        if (fNextId == std::numeric_limits<uint64_t>::max())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Renderer image IDs exhausted");
        const auto id = fNextId;
        fImages.emplace(id, RenderImageRecord{std::move(pixels), properties});
        ++fNextId;
        return RenderImage(*this, id);
    }
    void Renderer::SetImage(uint64_t id, const IMCodec::ImageSharedPtr& pixels)
    {
        assert(!fRendering);
        auto& record = fImages.at(id);
        if (record.pixels != pixels)
        {
            record.pixels = pixels;
            ++record.revision;
        }
    }
    void Renderer::SetProperties(uint64_t id, const RenderImageProperties& properties)
    {
        assert(!fRendering);
        fImages.at(id).properties = properties;
    }
    void Renderer::Release(uint64_t id) noexcept
    {
        assert(!fRendering);
        if (fBackend)
            fBackend->ReleaseImageResources(id);
        fImages.erase(id);
    }
    void Renderer::Redraw()
    {
        assert(!fRendering);
#ifndef NDEBUG
        // Scope reset also covers backend exceptions without retaining borrowed records.
        struct FrameScope
        {
            // Borrowed rendering guard reset when the frame exits.
            bool& rendering;
            ~FrameScope() { rendering = false; }
        } scope{fRendering};
        fRendering = true;
#endif
        if (fBackend)
            fBackend->Render(RenderFrame{fImages});
    }
    void Renderer::Shutdown() noexcept
    {
        assert(!fRendering);
        fBackend.reset();
    }
    void Renderer::SetViewParams(const ViewParameters& p)
    {
        fBackend->SetViewParams(p);
    }
    void Renderer::SetFilterLevel(OIV::FilterType f)
    {
        fBackend->SetFilterLevel(f);
    }
    void Renderer::SetExposure(const OIV::ColorExposure& e)
    {
        fBackend->SetExposure(e);
    }
    void Renderer::SetSelectionRect(VisualSelectionRect s)
    {
        fBackend->SetSelectionRect(s);
    }
    void Renderer::SetBackgroundColor(int i, LLUtils::Color c)
    {
        fBackend->SetBackgroundColor(i, c);
    }
    const RendererAdapter& Renderer::GetSelectedAdapter() const
    {
        return fBackend->GetSelectedAdapter();
    }
}  // namespace OIV
