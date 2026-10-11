#pragma once
#include <Defs.h>
#include <Image.h>
#include <vector>
#include <Interfaces/IRendererDefs.h>
#include <RendererCore/RenderFrame.h>

namespace OIV
{
    // Graphics contract selected and initialized by API composition.
    class IRenderBackend
    {
      public:

        virtual int Init(const OIV_RendererInitializationParams& initParams)      = 0;
        virtual int SetViewParams(const ViewParameters& viewParams)               = 0;
        virtual int Render(const RenderFrame& frame)                              = 0;
        virtual int SetFilterLevel(OIV_Filter_type filterType)                    = 0;
        virtual int SetExposure(const OIV_CMD_ColorExposure_Request& exposure)    = 0;
        virtual int SetSelectionRect(VisualSelectionRect selectionRect)           = 0;
        virtual int SetBackgroundColor(int index, LLUtils::Color backgroundColor) = 0;

        // Logical release must not throw. Backend ownership covers submitted work
        // until completion or device teardown, even after common records disappear.
        virtual void ReleaseImageResources(uint64_t id) noexcept = 0;

        // Enumerates API indices before initialization. Capability and initialization failures are
        // handled per candidate by the startup policy; enumeration never commits a selected device.
        virtual std::vector<RendererAdapter> EnumerateAdapters() { return {}; }
        virtual Acceleration GetAcceleration() const { return Acceleration::Unknown; }
        virtual const char* GetBackendName() const = 0;
        // Device names are UTF-8 on every platform.
        virtual const char* GetGPUName() const { return ""; }
        virtual const char* GetAPIVersion() const { return ""; }
        virtual const char* GetDriverVersion() const { return ""; }
        virtual int GetSelectedGPUIndex() const { return -1; }

        virtual ~IRenderBackend() {}
    };

    using RenderBackendSharedPtr = std::shared_ptr<IRenderBackend>;
}  // namespace OIV