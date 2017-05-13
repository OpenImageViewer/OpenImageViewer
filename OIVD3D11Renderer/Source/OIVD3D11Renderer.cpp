#include "OIVD3D11Renderer.h"
#include "D3D11/D3D11Renderer.h"

namespace OIV
{
    OIVD3D11Renderer::OIVD3D11Renderer()
    {
        fD3D11Renderer = std::unique_ptr<D3D11Renderer>(new D3D11Renderer());
    }

    int OIVD3D11Renderer::Init(std::size_t container)
    {
        return fD3D11Renderer->Init(container);
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

     int OIVD3D11Renderer::SetImage(const IMCodec::ImageSharedPtr image)
    {
         return fD3D11Renderer->SetImage(image);
    }
}
