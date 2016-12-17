#pragma once
#include "Image/Image.h"
#include "../ViewParameters.h"
#include "API/defs.h"

namespace OIV
{
    class IRenderer
    {
    public:
        virtual int Init(size_t container) = 0;
        virtual int SetImage(const ImageSharedPtr image) = 0;
        virtual int SetViewParams(const ViewParameters& viewParams) = 0;
        virtual int Redraw() = 0;
        virtual int SetFilterLevel(OIV_Filter_type filterType) = 0;
        virtual ~IRenderer() {}
    };

    typedef std::shared_ptr<IRenderer> IRendererSharedPtr;
}
    