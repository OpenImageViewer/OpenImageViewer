#pragma once
#include <interfaces/IRenderer.h>
#include <defs.h>
#include "D3D11/D3D11Renderer.h"

namespace OIV
{
    
    class OIVD3D11Renderer : public IRenderer
    {
    public:
        OIVD3D11Renderer();
        
#pragma region /****IRenderer Overrides************/
    public:
        int Init(const OIV_RendererInitializationParams& initParams) override;
        int SetViewParams(const ViewParameters& viewParams) override;
        void UpdateGpuParameters();
        int Redraw() override;
        int SetFilterLevel(OIV_Filter_type filterType) override;
        int SetSelectionRect(SelectionRect selectionRect) override;
        int SetExposure(const OIV_CMD_ColorExposure_Request& exposure) override;
        
        // Multi image API:
        int SetImageBuffer(uint32_t id, const IMCodec::ImageSharedPtr image) override;
        int SetImageProperties(const OIV_CMD_ImageProperties_Request& properties) override;
        int RemoveImage(uint32_t id) override;

#pragma endregion


    private:
        std::unique_ptr<D3D11Renderer> fD3D11Renderer;
        
    };

}
