#pragma once
#include "Interfaces/IRenderer.h"

namespace OIV
{
    class NullRenderer : public IRenderer
    {
    public:
        int Init(const OIV_RendererInitializationParams& initParams) override { return 0; }
        int SetViewParams(const ViewParameters& viewParams) { return 0; }
        int Redraw() override { return 0; }
        int SetFilterLevel(OIV_Filter_type filterLevel) override { return 0; }
    };
}
