#pragma once
#include "Interfaces/IRenderer.h"

namespace OIV
{
    class NullRenderer : public IRenderer
    {
    public:
        // Inherited via IRenderer
        int Init([[maybe_unused]] const OIV_RendererInitializationParams& initParams) override { return 0; }
        int SetViewParams([[maybe_unused]] const ViewParameters& viewParams) override { return 0; }
        int Redraw() override { return 0; }
        int SetFilterLevel([[maybe_unused]] OIV_Filter_type filterLevel) override { return 0; }
        int SetExposure([[maybe_unused]] const OIV_CMD_ColorExposure_Request & exposure) override { return 0; }
        int SetSelectionRect([[maybe_unused]] SelectionRect selectionRect) override { return 0; }
        int SetImageBuffer([[maybe_unused]] uint32_t id, [[maybe_unused]] const IMCodec::ImageSharedPtr image) override { return 0; }
        int SetImageProperties([[maybe_unused]] const OIV_CMD_ImageProperties_Request &) override { return 0; }
        int RemoveImage([[maybe_unused]] uint32_t id) override { return 0; }
    };
}
