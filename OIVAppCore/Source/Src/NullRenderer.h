#pragma once
#include "RendererCore/IRenderBackend.h"

namespace OIV
{
    class NullRenderer : public IRenderBackend
    {
      public:

        // Inherited via IRenderBackend
        void Init([[maybe_unused]] const OIV::RendererInitialization& initParams) override {}
        void SetViewParams([[maybe_unused]] const ViewParameters& viewParams) override {}
        void Render(const RenderFrame&) override {}
        void SetFilterLevel([[maybe_unused]] OIV::FilterType filterLevel) override {}
        void SetExposure([[maybe_unused]] const OIV::ColorExposure& exposure) override {}
        void SetSelectionRect([[maybe_unused]] VisualSelectionRect selectionRect) override {}
        void SetBackgroundColor(int index, LLUtils::Color backgroundColor) override {}

        void ReleaseImageResources(uint64_t) noexcept override {}
    };
}  // namespace OIV
