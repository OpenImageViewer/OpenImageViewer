#include "OIVD3D11Renderer.h"
#include "D3D11/D3D11Renderer.h"

#include <LLUtils/StringUtility.h>

namespace OIV
{
    OIVD3D11Renderer::OIVD3D11Renderer() : fD3D11Renderer(std::make_unique<D3D11Renderer>()) {}

    int OIVD3D11Renderer::SetSelectionRect(VisualSelectionRect selectionRect)
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
        return fD3D11Renderer->SetFilterLevel(filterType);
    }

    int OIVD3D11Renderer::SetExposure(const OIV_CMD_ColorExposure_Request& exposure)
    {
        return fD3D11Renderer->SetExposure(exposure);
    }

    int OIVD3D11Renderer::AddRenderable(IRenderable* renderable)
    {
        return fD3D11Renderer->AddRenderable(renderable);
    }
    int OIVD3D11Renderer::RemoveRenderable(IRenderable* renderable)
    {
        return fD3D11Renderer->RemoveRenderable(renderable);
    }

    int OIVD3D11Renderer::SetBackgroundColor(int index, LLUtils::Color backgroundColor)
    {
        return fD3D11Renderer->SetBackgroundColor(index, backgroundColor);
    }

    int OIVD3D11Renderer::GetSelectedGPUIndex() const
    {
        return fD3D11Renderer->GetSelectedGPUIndex();
    }

    const char* OIVD3D11Renderer::GetGPUName() const
    {
        if (fGPUName.empty())
        {
            const DXGI_ADAPTER_DESC desc = fD3D11Renderer->GetAdapterDesc();
            fGPUName                     = LLUtils::StringUtility::ConvertString<std::string>(desc.Description);
            if (fGPUName.empty())
                fGPUName = "Unknown";
        }
        return fGPUName.c_str();
    }

    const char* OIVD3D11Renderer::GetAPIVersion() const
    {
        if (fAPIVersion.empty())
        {
            fAPIVersion = "11.0";
        }
        return fAPIVersion.c_str();
    }

    const char* OIVD3D11Renderer::GetDriverVersion() const
    {
        if (fDriverVersion.empty())
        {
            const LARGE_INTEGER version = fD3D11Renderer->GetDriverVersion();
            if (version.QuadPart != 0)
            {
                fDriverVersion = std::to_string(HIWORD(version.HighPart)) + "." +
                                 std::to_string(LOWORD(version.HighPart)) + "." +
                                 std::to_string(HIWORD(version.LowPart)) + "." +
                                 std::to_string(LOWORD(version.LowPart));
            }
            else
            {
                fDriverVersion = "Unknown";
            }
        }
        return fDriverVersion.c_str();
    }

}  // namespace OIV
