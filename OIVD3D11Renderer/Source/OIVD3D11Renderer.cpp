#include "OIVD3D11Renderer.h"
#include "D3D11/D3D11Renderer.h"

namespace OIV
{
    OIVD3D11Renderer::OIVD3D11Renderer()
    {
        fD3D11Renderer = std::unique_ptr<D3D11Renderer>(new D3D11Renderer());
    }

    int OIVD3D11Renderer::SetSelectionRect(SelectionRect selectionRect)
    {
        return fD3D11Renderer->SetselectionRect(selectionRect);
    }
    int OIVD3D11Renderer::Init(const OIV_RendererInitializationParams& initParams)
    {
        return fD3D11Renderer->Init(initParams);
    }

     int OIVD3D11Renderer::SetViewParams(const ViewParameters& viewParams)
    {
         return fD3D11Renderer->SetViewParams(viewParams);
    }

     void OIVD3D11Renderer::UpdateGpuParameters()
    {
         fD3D11Renderer->UpdateGpuParameters();
    }

     int OIVD3D11Renderer::Redraw()
    {
         return fD3D11Renderer->Redraw();
    }

     int OIVD3D11Renderer::SetFilterLevel(OIV_Filter_type filterType)
    {
         return  fD3D11Renderer->SetFilterLevel(filterType);
    }

    int OIVD3D11Renderer::SetExposure(const OIV_CMD_ColorExposure_Request& exposure)
    {
        return fD3D11Renderer->SetExposure(exposure);
    }

    int OIVD3D11Renderer::SetImageBuffer(uint32_t id, const IMCodec::ImageSharedPtr image)
    {
        return fD3D11Renderer->SetImageBuffer(id, image);
    }
    int OIVD3D11Renderer::SetImageProperties(const OIV_CMD_ImageProperties_Request& props)
    {
        return fD3D11Renderer->SetImageProperties(props);
    }

    int OIVD3D11Renderer::RemoveImage(uint32_t id)
    {
        return fD3D11Renderer->RemoveImage(id);
    }

}
