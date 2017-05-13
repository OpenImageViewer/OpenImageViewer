#pragma once
#include "../../oiv/interfaces/IRenderer.h"
#include "../../oiv/API/defs.h"
#include "D3D11/D3D11Renderer.h"

namespace OIV
{
    
    class OIVD3D11Renderer : public IRenderer
    {
    public:
        OIVD3D11Renderer();
#pragma region /****IRenderer Overrides************/
    public:
        int Init(std::size_t container) override;
        int SetViewParams(const ViewParameters& viewParams) override;
        void UpdateGpuParameters();
        int Redraw() override;
        int SetFilterLevel(OIV_Filter_type filterType) override;
        int SetImage(const IMCodec::ImageSharedPtr image) override;

#pragma endregion


    private:
        std::unique_ptr<D3D11Renderer> fD3D11Renderer;
        
    };

}
