#pragma once
#include "RendererCore/IRenderBackend.h"

namespace OIV
{
    class NullRenderer : public IRenderBackend
    {
      public:

        // Inherited via IRenderBackend
        int Init([[maybe_unused]] const OIV_RendererInitializationParams& initParams) override { return 0; }
        int SetViewParams([[maybe_unused]] const ViewParameters& viewParams) override { return 0; }
        int Render(const RenderFrame&) override { return 0; }
        int SetFilterLevel([[maybe_unused]] OIV_Filter_type filterLevel) override { return 0; }
        int SetExposure([[maybe_unused]] const OIV_CMD_ColorExposure_Request& exposure) override { return 0; }
        int SetSelectionRect([[maybe_unused]] VisualSelectionRect selectionRect) override { return 0; }
        int SetBackgroundColor(int index, LLUtils::Color backgroundColor) override { return 0; }

        void ReleaseImageResources(uint64_t) noexcept override {}

        const char* GetBackendName() const override { return "Null"; }
    };
}  // namespace OIV
