#pragma once
#include <defs.h>
#include <Image.h>
#include "IRendererDefs.h"
#include "IRenderable.h"

namespace OIV
{
    class IRenderer
    {
    public:
        virtual int Init(const OIV_RendererInitializationParams& initParams) = 0;
        virtual int SetViewParams(const ViewParameters& viewParams) = 0;
        virtual int Redraw() = 0;
        virtual int SetFilterLevel(OIV_Filter_type filterType) = 0;
        virtual int SetExposure(const OIV_CMD_ColorExposure_Request& exposure) = 0;
        virtual int SetSelectionRect(VisualSelectionRect selectionRect) = 0;
        virtual int SetBackgroundColor(int index, LLUtils::Color backgroundColor) = 0;

        virtual int AddRenderable(IRenderable* renderable) = 0;
        virtual int RemoveRenderable(IRenderable* renderable) = 0;

        virtual ~IRenderer() {}
    };

    typedef std::shared_ptr<IRenderer> IRendererSharedPtr;
}    