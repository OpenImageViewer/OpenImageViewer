#include <filesystem>
#include <d3dcommon.h>
#include <d3d11.h>
#include <LLUtils/PlatformUtility.h>
#include "D3D11Renderer.h"
#include "D3D11Common.h"
#include "D3D11VertexShader.h"
#include "D3D11FragmentShader.h"
#include "D3D11Utility.h"
#include "../OIVD3DHelper.h"

namespace OIV
{
    void D3D11Renderer::SetDeviceState()
    {
        ID3D11DeviceContext* d3dContext = fDevice->GetContext();
        fImageVertexShader->Use();
        d3dContext->IASetInputLayout(fInputLayout.Get());
        // Set quad vertex buffer
        UINT stride = 8;
        UINT offset = 0;
        d3dContext->IASetVertexBuffers(0, 1, fVertexBuffer.GetAddressOf(), &stride, &offset);
        d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        d3dContext->OMSetBlendState(fBlendState.Get(), fBackgroundColor.channels.data(), static_cast<UINT>(0xFFFFFFFF));
        d3dContext->PSSetSamplers(static_cast<UINT>(0), static_cast<UINT>(1), fSamplerState.GetAddressOf());

        d3dContext->RSSetViewports(1, &fViewport);
    }

    void D3D11Renderer::ResizeBackBuffer(int x, int y)
    {
        IDXGISwapChain* d3dSwapChain    = fDevice->GetSwapChain();
        ID3D11DeviceContext* d3dContext = fDevice->GetContext();
        ID3D11Device* d3dDevice         = fDevice->GetdDevice();

        fRenderTargetView.Reset();
        D3D11Error::HandleDeviceError(d3dSwapChain->ResizeBuffers(2, x, y, DXGI_FORMAT_UNKNOWN, static_cast<UINT>(0)),
                                      "can not resize swap chain");

        ComPtr<ID3D11Texture2D> backbuffer;

        LLUTILS_DISABLE_WARNING_PUSH
        LLUTILS_DISABLE_WARNING_LANGUAGE_EXTENSION

        D3D11Error::HandleDeviceError(d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                                                              reinterpret_cast<void**>(backbuffer.GetAddressOf())),
                                      "Can not retrieve back buffer");

        LLUTILS_DISABLE_WARNING_POP
        // create a render target pointing to the back buffer
        D3D11Error::HandleDeviceError(d3dDevice->CreateRenderTargetView(backbuffer.Get(), nullptr,
                                                                        fRenderTargetView.GetAddressOf()),
                                      " Can not create render target view");

        d3dContext->OMSetRenderTargets(1, fRenderTargetView.GetAddressOf(), nullptr);
        d3dContext->ClearRenderTargetView(fRenderTargetView.Get(), fBackgroundColor.channels.data());
    }

    void D3D11Renderer::CreateBuffers()
    {
        /* Create vertex buffer*/

        // Front face - clock wise
        float quad[] = {
            -1,
            1  // top left corner
            ,
            1,
            1  // top right corner
            ,
            -1,
            -1  // bottom left corner
            ,
            1,
            -1  // bottom right corner
        };

        // Fill in a buffer description.
        D3D11_BUFFER_DESC bufferDesc;
        bufferDesc.Usage          = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth      = sizeof(quad);
        bufferDesc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = 0;
        bufferDesc.MiscFlags      = 0;

        // Fill in the subresource data.
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem          = quad;
        InitData.SysMemPitch      = 0;
        InitData.SysMemSlicePitch = 0;

        // Create the vertex buffer.
        HRESULT res = fDevice->GetdDevice()->CreateBuffer(&bufferDesc, &InitData,
                                                          fVertexBuffer.ReleaseAndGetAddressOf());

        D3D11Error::HandleDeviceError(res, "Could not create buffer");

        // create buffer input layout

        D3D11_INPUT_ELEMENT_DESC ied[] = {
            {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
        };

        D3D11Error::HandleDeviceError(
            fDevice->GetdDevice()->CreateInputLayout(ied, 1, fImageVertexShader->GetShaderData().data(),
                                                     fImageVertexShader->GetShaderData().Size(),
                                                     fInputLayout.ReleaseAndGetAddressOf()),
            "Could not crate Input layout");

        //******** Create constant buffer *********/

        {
            // Fill in a buffer description.
            D3D11_BUFFER_DESC cbDesc;
            cbDesc.ByteWidth           = LLUtils::Utility::Align<UINT>(sizeof(CONSTANT_BUFFER_IMAGE_MAIN), 16);
            cbDesc.Usage               = D3D11_USAGE_DYNAMIC;
            cbDesc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
            cbDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
            cbDesc.MiscFlags           = 0;
            cbDesc.StructureByteStride = 0;

            // Fill in the subresource data.
            /*D3D11_SUBRESOURCE_DATA InitData;
            InitData.pSysMem = &fShaderParameters;
            InitData.SysMemPitch = 0;
            InitData.SysMemSlicePitch = 0;*/

            fBufferImageMain = std::make_unique<D3D11BufferBound<CONSTANT_BUFFER_IMAGE_MAIN>>(fDevice, cbDesc, nullptr);

            CONSTANT_BUFFER_IMAGE_MAIN& buffer = fBufferImageMain->GetBuffer();
            buffer.exposure                    = 1.0;
            buffer.gamma                       = 1.0;
            buffer.offset                      = 0.0;
            buffer.saturation                  = 1.0;
        }

        // Create constant buffer for selection rect.
        {
            // Fill in a buffer description.
            D3D11_BUFFER_DESC cbDesc;
            cbDesc.ByteWidth           = sizeof(CONSTANT_BUFFER_SELECTION_RECT);
            cbDesc.Usage               = D3D11_USAGE_DYNAMIC;
            cbDesc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
            cbDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
            cbDesc.MiscFlags           = 0;
            cbDesc.StructureByteStride = 0;

            // Fill in the subresource data.
            /*  D3D11_SUBRESOURCE_DATA InitData;
              InitData.pSysMem = &fShaderParametersSelectionRect;
              InitData.SysMemPitch = 0;
              InitData.SysMemSlicePitch = 0;*/

            fBufferSelection = std::make_unique<D3D11BufferBound<CONSTANT_BUFFER_SELECTION_RECT>>(fDevice, cbDesc,
                                                                                                  nullptr);

            // Mark cpu buffer as invalid selection rect.
            fBufferSelection->GetBuffer().uSelectionRect[0] = -1;
        }

        {
            D3D11_BUFFER_DESC cbDesc;
            cbDesc.ByteWidth           = LLUtils::Utility::Align<UINT>(sizeof(CONSTANT_BUFFER_GLOBALS), 16);
            cbDesc.Usage               = D3D11_USAGE_DYNAMIC;
            cbDesc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
            cbDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
            cbDesc.MiscFlags           = 0;
            cbDesc.StructureByteStride = 0;

            fBufferGlobals = std::make_unique<D3D11BufferBound<CONSTANT_BUFFER_GLOBALS>>(fDevice, cbDesc, nullptr);

            memcpy(fBufferGlobals->GetBuffer().backgroundColor1, fBackgroundColors[0].channels.data(),
                   sizeof(float) * 4);
            memcpy(fBufferGlobals->GetBuffer().backgroundColor2, fBackgroundColors[1].channels.data(),
                   sizeof(float) * 4);
        }

        // Create constant buffer for selection rect.
        {
            // Fill in a buffer description.
            D3D11_BUFFER_DESC cbDesc;
            cbDesc.ByteWidth           = LLUtils::Utility::Align<UINT>(sizeof(CONSTANT_BUFFER_IMAGE_COMMON), 16);
            cbDesc.Usage               = D3D11_USAGE_DYNAMIC;
            cbDesc.BindFlags           = D3D11_BIND_CONSTANT_BUFFER;
            cbDesc.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
            cbDesc.MiscFlags           = 0;
            cbDesc.StructureByteStride = 0;

            fBufferImageCommon = std::make_unique<D3D11BufferBound<CONSTANT_BUFFER_IMAGE_COMMON>>(fDevice, cbDesc,
                                                                                                  nullptr);
        }

        D3D11_SAMPLER_DESC sampler;
        D3D11Utility::CreateD3D11DefaultSamplerState(sampler);

        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateSamplerState(&sampler,
                                                                                fSamplerState.ReleaseAndGetAddressOf()),
                                      "Could not create sampler state");

        OIV_D3D_SET_OBJECT_NAME(fSamplerState, "Sampler state");

        D3D11_BLEND_DESC blend;
        D3D11Utility::CreateD3D11DefaultBlendState(blend);

        // modify to alpha blend.
        blend.RenderTarget[0].BlendEnable = TRUE;
        blend.RenderTarget[0].BlendOp     = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlend    = D3D11_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend   = D3D11_BLEND_INV_SRC_ALPHA;

        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateBlendState(&blend,
                                                                              fBlendState.ReleaseAndGetAddressOf()),
                                      "Could not create blend state");
    }

    void D3D11Renderer::CreateShaders()
    {
        using path = std::filesystem::path;

        // A WLX renderer lives in a different module from the host executable. Resolve shader source
        // beside this module so a cold plugin cache never depends on the standalone application's bytecode.
        const path programsPath = path(LLUtils::PlatformUtility::GetDllFolder()) / "Resources" / "Programs";

        fImageVertexShader = std::make_unique<D3D11VertexShader>(fDevice);
        fImageVertexShader->SetSourceFileName(programsPath / L"QuadVP.shader");
        fImageFragmentShader = std::make_unique<D3D11FragmentShader>(fDevice);
        fImageFragmentShader->SetSourceFileName(programsPath / L"QuadFP.shader");
        fSelectionFragmentShader = std::make_unique<D3D11FragmentShader>(fDevice);
        fSelectionFragmentShader->SetSourceFileName(programsPath / L"QuadSelectionFP.shader");
        fImageSimpleFragmentShader = std::make_unique<D3D11FragmentShader>(fDevice);
        fImageSimpleFragmentShader->SetSourceFileName(programsPath / L"QuadSimpleFP.shader");

#ifdef _DEBUG
        constexpr auto buildMode = "Debug";
#else
        constexpr auto buildMode = "Release";
#endif
        const auto shaderCachePath = fCacheDirectory.empty() ? path{} : fCacheDirectory / buildMode;

        D3D11Utility::LoadShader(fImageVertexShader, shaderCachePath);
        D3D11Utility::LoadShader(fImageFragmentShader, shaderCachePath);
        D3D11Utility::LoadShader(fSelectionFragmentShader, shaderCachePath);
        D3D11Utility::LoadShader(fImageSimpleFragmentShader, shaderCachePath);
    }

    void D3D11Renderer::Init(const OIV::RendererInitialization& initParams)
    {
        fCacheDirectory = initParams.cacheDirectory;
        fDevice         = std::make_shared<D3D11Device>();
        fDevice->Create(reinterpret_cast<HWND>(initParams.container), initParams.gpuIndex, initParams.adapterName,
                        initParams.acceleration);

        ResizeBackBuffer(1280, 800);
        CreateShaders();
        CreateBuffers();
        SetDeviceState();
    }

    void D3D11Renderer::UpdateViewportSize(int x, int y)
    {
        fViewport.Width  = static_cast<FLOAT>(x);
        fViewport.Height = static_cast<FLOAT>(y);
        fIsParamsDirty   = true;

        ResizeBackBuffer(x, y);
        fDevice->GetContext()->RSSetViewports(1, &fViewport);
    }

    void D3D11Renderer::SetViewParams(const ViewParameters& viewParams)
    {
        UpdateViewportSize(static_cast<int>(viewParams.uViewportSize.x), static_cast<int>(viewParams.uViewportSize.y));
        CONSTANT_BUFFER_IMAGE_MAIN& buffer = fBufferImageMain->GetBuffer();

        buffer.uShowGrid = viewParams.showGrid;
        memcpy(buffer.transparencyColor1,
               static_cast<LLUtils::ColorF32>(viewParams.uTransparencyColor1).channels.data(), sizeof(float) * 4);
        memcpy(buffer.transparencyColor2,
               static_cast<LLUtils::ColorF32>(viewParams.uTransparencyColor2).channels.data(), sizeof(float) * 4);

        fIsParamsDirty = true;
    }

    void D3D11Renderer::UpdateGpuParameters()
    {
        if (fIsParamsDirty)
        {
            fBufferImageMain->Update();
            // fConstantBuffer->Write(sizeof(fShaderParameters), reinterpret_cast<const uint8_t*>(&fShaderParameters)
            //     , 0);
            fIsParamsDirty = false;
        }
        if (fGlobalsDirty)
        {
            fBufferGlobals->Update();
            fGlobalsDirty = false;
        }
    }

    void D3D11Renderer::DrawImage(ImageEntry& entry, const RenderImageRecord& record)
    {
        if (!record.pixels || record.properties.opacity <= 0.0 || !record.properties.visible)
            return;

        // TODO: change to switch statement and unify constant buffres.

        if (entry.uploadedRevision != record.revision || !entry.texture)
        {
            auto texture = OIVD3DHelper::CreateTexture(fDevice, record.pixels, false);
            if (!texture)
                throw RendererException(RendererErrorCode::BackendFailure, RendererType::D3D11, "Create D3D11 texture");
            entry.texture          = std::move(texture);
            entry.uploadedRevision = record.revision;

            if (entry.texture == nullptr)
                throw RendererException(RendererErrorCode::BackendFailure, RendererType::D3D11, "Create D3D11 texture");
        }

        CONSTANT_BUFFER_IMAGE_COMMON& gpuBuffer = fBufferImageCommon->GetBuffer();
        gpuBuffer.uImageOffset[0]               = static_cast<float>(record.properties.position.x);
        gpuBuffer.uImageOffset[1]               = static_cast<float>(record.properties.position.y);
        gpuBuffer.uvViewportSize[0]             = static_cast<float>(fViewport.Width);
        gpuBuffer.uvViewportSize[1]             = static_cast<float>(fViewport.Height);
        gpuBuffer.uvViewportSize[2]             = static_cast<float>(1.0 / fViewport.Width);
        gpuBuffer.uvViewportSize[3]             = static_cast<float>(1.0 / fViewport.Height);
        gpuBuffer.uImageSize[0]                 = static_cast<float>(entry.texture->GetCreateParams().width);
        gpuBuffer.uImageSize[1]                 = static_cast<float>(entry.texture->GetCreateParams().height);
        gpuBuffer.uScale[0]                     = static_cast<float>(record.properties.scale.x);
        gpuBuffer.uScale[1]                     = static_cast<float>(record.properties.scale.y);
        gpuBuffer.opacity                       = static_cast<float>(record.properties.opacity);

        // TODO: Analyze whether caching the last uploaded constant-buffer data is worthwhile.
        // Efficient retained-mode client usage avoids updates when state is unchanged,
        // so identical consecutive draw payloads are expected to be uncommon. Verify this
        // expectation, including content-only changes and shared-buffer reuse across images.
        // Document the decision to implement or omit caching, with its evidence and rationale.
        fBufferImageCommon->Update();
        fBufferImageCommon->Use(ShaderStage::FragmentShader, 0);

        switch (record.properties.imageRenderMode)
        {
            case OIV::ImageRenderMode::Overlay:
                fImageSimpleFragmentShader->Use();
                break;
            case OIV::ImageRenderMode::MainImage:
                fBufferImageMain->Use(ShaderStage::FragmentShader, 1);
                fImageFragmentShader->Use();
                break;
            default:
                LL_EXCEPTION_UNEXPECTED_VALUE;
        }

        SetFilterLevel(record.properties.filterType);
        entry.texture->Use();
        fDevice->GetContext()->Draw(4, 0);
    }

    void D3D11Renderer::Render(const RenderFrame& frame)
    {
        UpdateGpuParameters();
        ID3D11DeviceContext* context = fDevice->GetContext();

        fBufferGlobals->Use(ShaderStage::FragmentShader, 2);
        // Draw images
        for (const auto& [id, record] : frame.images)
            if (record.pixels && record.properties.visible && record.properties.opacity > 0.0 &&
                record.properties.imageRenderMode == OIV::ImageRenderMode::MainImage)
                DrawImage(fImageEntries.try_emplace(id).first->second, record);

        // Draw selection rect.
        if (fBufferSelection->GetBuffer().uSelectionRect[0] != -1)
        {
            CONSTANT_BUFFER_SELECTION_RECT& buffer = fBufferSelection->GetBuffer();
            if (static_cast<int32_t>(fViewport.Width) != buffer.uvViewportSize[0] ||
                static_cast<int32_t>(fViewport.Height) != buffer.uvViewportSize[1])
            {
                buffer.uvViewportSize[0] = static_cast<int32_t>(fViewport.Width);
                buffer.uvViewportSize[1] = static_cast<int32_t>(fViewport.Height);
                fBufferSelection->Update();
            }

            fBufferSelection->Use(ShaderStage::FragmentShader, 0);
            fSelectionFragmentShader->Use();
            context->Draw(4, 0);
        }

        // Draw Overlays
        for (const auto& [id, record] : frame.images)
            if (record.pixels && record.properties.visible && record.properties.opacity > 0.0 &&
                record.properties.imageRenderMode == OIV::ImageRenderMode::Overlay)
                DrawImage(fImageEntries.try_emplace(id).first->second, record);

        D3D11Error::HandleDeviceError(fDevice->GetSwapChain()->Present(0, 0), "Present frame");
    }

    void D3D11Renderer::SetFilterLevel(OIV::FilterType filterType)
    {
        D3D11_SAMPLER_DESC desc;
        D3D11Utility::CreateD3D11DefaultSamplerState(desc);

        switch (filterType)
        {
            case OIV::FilterType::None:
                desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
                break;
            case OIV::FilterType::Linear:
                desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                break;
            case OIV::FilterType::Lanczos3:
                desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
                break;
            default:
                LL_EXCEPTION_UNEXPECTED_VALUE;
        }
        ComPtr<ID3D11SamplerState> replacement;
        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateSamplerState(&desc, replacement.GetAddressOf()),
                                      "Set image filter");
        fDevice->GetContext()->PSSetSamplers(0, 1, replacement.GetAddressOf());
        fSamplerState = std::move(replacement);
    }

    void D3D11Renderer::SetSelectionRect(VisualSelectionRect selection_rect)
    {
        fSelectionRect                         = selection_rect;
        CONSTANT_BUFFER_SELECTION_RECT& buffer = fBufferSelection->GetBuffer();

        buffer.uvViewportSize[0] = static_cast<int32_t>(fViewport.Width);
        buffer.uvViewportSize[1] = static_cast<int32_t>(fViewport.Height);
        buffer.uSelectionRect[0] = fSelectionRect.p0.x;
        buffer.uSelectionRect[1] = fSelectionRect.p0.y;
        buffer.uSelectionRect[2] = fSelectionRect.p1.x;
        buffer.uSelectionRect[3] = fSelectionRect.p1.y;
        fBufferSelection->Update();
    }

    void D3D11Renderer::SetExposure(const OIV::ColorExposure& exposure)
    {
        CONSTANT_BUFFER_IMAGE_MAIN& buffer = fBufferImageMain->GetBuffer();
        buffer.exposure                    = static_cast<float>(exposure.exposure);
        buffer.offset                      = static_cast<float>(exposure.offset);
        buffer.gamma                       = static_cast<float>(exposure.gamma);
        buffer.saturation                  = static_cast<float>(exposure.saturation);

        fIsParamsDirty = true;
    }

    void D3D11Renderer::ReleaseImageResources(uint64_t id) noexcept
    {
        fImageEntries.erase(id);
    }

    void D3D11Renderer::SetBackgroundColor(int index, LLUtils::Color backgroundColor)
    {
        fBackgroundColors.at(index) = static_cast<LLUtils::ColorF32>(backgroundColor);
        if (index == 0)
            memcpy(fBufferGlobals->GetBuffer().backgroundColor1, fBackgroundColors[0].channels.data(),
                   sizeof(float) * 4);
        else
            memcpy(fBufferGlobals->GetBuffer().backgroundColor2, fBackgroundColors[1].channels.data(),
                   sizeof(float) * 4);
        fGlobalsDirty = true;
    }

}  // namespace OIV