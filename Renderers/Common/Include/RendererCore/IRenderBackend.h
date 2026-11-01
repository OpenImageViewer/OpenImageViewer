#pragma once
#include <RendererCore/RenderTypes.h>
#include <Image.h>
#include <vector>
#include <RendererCore/ViewParameters.h>
#include <RendererCore/RenderFrame.h>

namespace OIV
{
    // Graphics contract selected and initialized by API composition.
    class IRenderBackend
    {
      public:

        virtual void Init(const OIV::RendererInitialization& initParams)           = 0;
        virtual void SetViewParams(const ViewParameters& viewParams)               = 0;
        virtual void Render(const RenderFrame& frame)                              = 0;
        virtual void SetFilterLevel(OIV::FilterType filterType)                    = 0;
        virtual void SetExposure(const OIV::ColorExposure& exposure)               = 0;
        virtual void SetSelectionRect(VisualSelectionRect selectionRect)           = 0;
        virtual void SetBackgroundColor(int index, LLUtils::Color backgroundColor) = 0;

        // Logical release must not throw. Backend ownership covers submitted work
        // until completion or device teardown, even after common records disappear.
        virtual void ReleaseImageResources(uint64_t id) noexcept = 0;

        // Enumerates API indices before initialization. Capability and initialization failures are
        // handled per candidate by the startup policy; enumeration never commits a selected device.
        virtual std::vector<RendererAdapter> EnumerateAdapters() { return {}; }
        // Actual selected device, empty before selection. Enumeration does not change it.
        // Borrowed from this backend until its next initialization or teardown.
        virtual const RendererAdapter& GetSelectedAdapter() const
        {
            static const RendererAdapter empty;
            return empty;
        }
        virtual ~IRenderBackend() {}
    };

}  // namespace OIV