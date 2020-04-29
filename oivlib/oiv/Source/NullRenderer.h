#pragma once
#include "Interfaces/IRenderer.h"

namespace OIV
{
    class NullRenderer : public IRenderer
    {
    public:
        // Inherited via IRenderer
        int Init(const OIV_RendererInitializationParams& initParams) override { return 0; }
        int SetViewParams(const ViewParameters& viewParams) { return 0; }
        int Redraw() override { return 0; }
        int SetFilterLevel(OIV_Filter_type filterLevel) override { return 0; }
        int SetExposure(const OIV_CMD_ColorExposure_Request & exposure) override { return 0; }
        int SetSelectionRect(SelectionRect selectionRect) override { return 0; }
        int SetImageBuffer(uint32_t id, const IMCodec::ImageSharedPtr image) override { return 0; }
        int SetImageProperties(const OIV_CMD_ImageProperties_Request &) override { return 0; }
        int RemoveImage(uint32_t id) override { return 0; }
    };
}
