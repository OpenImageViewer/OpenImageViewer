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
    Renderer::Renderer(RenderBackendSharedPtr backend) : fBackend(std::move(backend))
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
    int Renderer::Redraw()
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
        return fBackend ? fBackend->Render(RenderFrame{fImages}) : 0;
    }
    void Renderer::Shutdown() noexcept
    {
        assert(!fRendering);
        fBackend.reset();
    }
    int Renderer::SetViewParams(const ViewParameters& p)
    {
        fViewParams = p;
        return fBackend->SetViewParams(fViewParams);
    }
    int Renderer::SetFilterLevel(OIV_Filter_type f)
    {
        return fBackend->SetFilterLevel(f);
    }
    int Renderer::SetExposure(const OIV_CMD_ColorExposure_Request& e)
    {
        return fBackend->SetExposure(e);
    }
    int Renderer::SetSelectionRect(VisualSelectionRect s)
    {
        return fBackend->SetSelectionRect(s);
    }
    int Renderer::SetBackgroundColor(int i, LLUtils::Color c)
    {
        return fBackend->SetBackgroundColor(i, c);
    }
    Acceleration Renderer::GetAcceleration() const
    {
        return fBackend->GetAcceleration();
    }
    const char* Renderer::GetBackendName() const
    {
        return fBackend->GetBackendName();
    }
    const char* Renderer::GetGPUName() const
    {
        return fBackend->GetGPUName();
    }
    const char* Renderer::GetAPIVersion() const
    {
        return fBackend->GetAPIVersion();
    }
    const char* Renderer::GetDriverVersion() const
    {
        return fBackend->GetDriverVersion();
    }
    int Renderer::GetSelectedGPUIndex() const
    {
        return fBackend->GetSelectedGPUIndex();
    }
}  // namespace OIV
