#pragma once
#include <RendererCore/IRenderBackend.h>
#include <Defs.h>
#include "D3D11/D3D11Renderer.h"

namespace OIV
{

    // Selected Direct3D backend forwarding frames and resource retirement.
    class OIVD3D11Renderer : public IRenderBackend
    {
      public:

        OIVD3D11Renderer();

#pragma region /****IRenderBackend Overrides************/

      public:

        int Init(const OIV_RendererInitializationParams& initParams) override;
        int SetViewParams(const ViewParameters& viewParams) override;
        void UpdateGpuParameters();
        int Render(const RenderFrame& frame) override;
        int SetFilterLevel(OIV_Filter_type filterType) override;
        int SetSelectionRect(VisualSelectionRect selectionRect) override;
        int SetExposure(const OIV_CMD_ColorExposure_Request& exposure) override;
        int SetBackgroundColor(int index, LLUtils::Color backgroundColor) override;
        void ReleaseImageResources(uint64_t id) noexcept override;

        const char* GetBackendName() const override { return "D3D11"; }
        std::vector<RendererAdapter> EnumerateAdapters() override { return D3D11Device::EnumerateAdapters(); }
        Acceleration GetAcceleration() const override { return fD3D11Renderer->GetAcceleration(); }
        int GetSelectedGPUIndex() const override;
        const char* GetGPUName() const override;
        const char* GetAPIVersion() const override;
        const char* GetDriverVersion() const override;

#pragma endregion

      private:

        // Owned Direct3D implementation behind this forwarding backend.
        std::unique_ptr<D3D11Renderer> fD3D11Renderer;
        // Cached UTF-8 selected GPU name.
        mutable std::string fGPUName;
        // Cached UTF-8 graphics API version.
        mutable std::string fAPIVersion;
        // Cached UTF-8 selected driver version.
        mutable std::string fDriverVersion;
    };

}  // namespace OIV
