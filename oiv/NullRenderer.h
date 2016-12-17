#pragma once
#include "interfaces/IRenderer.h"

namespace OIV
{
    class NullRenderer : public IRenderer
    {
    public:
        int Init(size_t container) override { return 0; }
        int SetViewParams(const ViewParameters& viewParams) { return 0; }
        int Redraw() override { return 0; }
        int SetFilterLevel(OIV_Filter_type filterLevel) override { return 0; }
        int SetImage(const ImageSharedPtr image) override { return 0; }
    };
}
