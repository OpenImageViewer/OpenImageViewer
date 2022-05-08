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
    D3D11Renderer::D3D11Renderer()
    {
        fViewport.MinDepth = 0;
        fViewport.MaxDepth = 1;
    }

    void D3D11Renderer::SetDevicestate()
    {
        ID3D11DeviceContext* d3dContext = fDevice->GetContext();
        fImageVertexShader->Use();
        d3dContext->IASetInputLayout(fInputLayout.Get());
        //Set quad vertex buffer
        UINT stride = 8;
        UINT offset = 0;
        d3dContext->IASetVertexBuffers(0, 1, fVertexBuffer.GetAddressOf(), &stride, &offset);
        d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
        
        d3dContext->OMSetBlendState(fBlendState.Get(), fBackgroundColor.GetNormalizedColorValue<FLOAT>().data() , static_cast<UINT>(0xFFFFFFFF));
        d3dContext->PSSetSamplers(static_cast<UINT>(0), static_cast<UINT>(1), fSamplerState.GetAddressOf());
        
        d3dContext->RSSetViewports(1, &fViewport);
    }

    void D3D11Renderer::ResizeBackBuffer(int x, int y)
    {
        IDXGISwapChain* d3dSwapChain = fDevice->GetSwapChain();
        ID3D11DeviceContext* d3dContext = fDevice->GetContext();
        ID3D11Device* d3dDevice = fDevice->GetdDevice();
        
        fRenderTargetView.Reset();
        D3D11Error::HandleDeviceError(d3dSwapChain->ResizeBuffers(2, x, y, DXGI_FORMAT_UNKNOWN, static_cast<UINT>(0)),
            "can not resize swap chain");
        
        ComPtr<ID3D11Texture2D> backbuffer;
        
LLUTILS_DISABLE_WARNING_PUSH
LLUTILS_DISABLE_WARNING_LANGUAGE_EXTENSION

        D3D11Error::HandleDeviceError(
            d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(backbuffer.GetAddressOf()))
            ,"Can not retrieve back buffer");

LLUTILS_DISABLE_WARNING_POP
        // create a render target pointing to the back buffer
        D3D11Error::HandleDeviceError(d3dDevice->CreateRenderTargetView(backbuffer.Get(), nullptr, fRenderTargetView.GetAddressOf())
        ," Can not create render target view");

        
        d3dContext->OMSetRenderTargets(1, fRenderTargetView.GetAddressOf(), nullptr);
        d3dContext->ClearRenderTargetView(fRenderTargetView.Get(), fBackgroundColor.GetNormalizedColorValue<FLOAT>().data());
    }

    void D3D11Renderer::CreateBuffers()
    {
        /* Create vertex buffer*/

        // Front face - clock wise
        float quad[] =
        {
              -1,  1 // top left corner
            ,  1,  1 // top right corner
            , -1, -1 // bottom left corner
            ,  1, -1 // bottom right corner
        };


        // Fill in a buffer description.
        D3D11_BUFFER_DESC bufferDesc;
        bufferDesc.Usage = D3D11_USAGE_DEFAULT;
        bufferDesc.ByteWidth = sizeof(quad);
        bufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bufferDesc.CPUAccessFlags = 0;
        bufferDesc.MiscFlags = 0;

        // Fill in the subresource data.
        D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = quad;
        InitData.SysMemPitch = 0;
        InitData.SysMemSlicePitch = 0;

        // Create the vertex buffer.
        HRESULT res = fDevice->GetdDevice()->CreateBuffer(&bufferDesc, &InitData, fVertexBuffer.ReleaseAndGetAddressOf());

        D3D11Error::HandleDeviceError(res, "Could not create buffer");

        // create buffer input layout

        D3D11_INPUT_ELEMENT_DESC ied[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

    D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateInputLayout(ied
        , 1
            , fImageVertexShader->GetShaderData().data()
            , fImageVertexShader->GetShaderData().Size(), fInputLayout.ReleaseAndGetAddressOf())
    , "Could not crate Input layout");

        //******** Create constant buffer *********/

    {
        // Fill in a buffer description.
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = LLUtils::Utility::Align<UINT>(sizeof(CONSTANT_BUFFER_IMAGE_MAIN), 16);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        cbDesc.StructureByteStride = 0;


        // Fill in the subresource data.
        /*D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = &fShaderParameters;
        InitData.SysMemPitch = 0;
        InitData.SysMemSlicePitch = 0;*/

        fBufferImageMain = std::make_unique<D3D11BufferBound<CONSTANT_BUFFER_IMAGE_MAIN>>(fDevice, cbDesc, nullptr);

        CONSTANT_BUFFER_IMAGE_MAIN& buffer = fBufferImageMain->GetBuffer();
        buffer.exposure = 1.0;
        buffer.gamma = 1.0;
        buffer.offset = 0.0;
        buffer.saturation = 1.0;
    }


            //Create constant buffer for selection rect.
    {
        // Fill in a buffer description.
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = sizeof(CONSTANT_BUFFER_SELECTION_RECT);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        cbDesc.StructureByteStride = 0;


        // Fill in the subresource data.
      /*  D3D11_SUBRESOURCE_DATA InitData;
        InitData.pSysMem = &fShaderParametersSelectionRect;
        InitData.SysMemPitch = 0;
        InitData.SysMemSlicePitch = 0;*/

        fBufferSelection = std::make_unique<D3D11BufferBound<CONSTANT_BUFFER_SELECTION_RECT>>(fDevice, cbDesc, nullptr);

        // Mark cpu buffer as invalid selection rect.
        fBufferSelection->GetBuffer().uSelectionRect[0] = -1;
    }

    //Create constant buffer for selection rect.
    {
        // Fill in a buffer description.
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = LLUtils::Utility::Align<UINT> (sizeof(CONSTANT_BUFFER_IMAGE_COMMON), 16);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        cbDesc.StructureByteStride = 0;
        
        fBufferImageCommon = std::make_unique<D3D11BufferBound<CONSTANT_BUFFER_IMAGE_COMMON>>(fDevice, cbDesc, nullptr);
       
    }
        

        D3D11_SAMPLER_DESC sampler;
        D3D11Utility::CreateD3D11DefaultSamplerState(sampler);

        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateSamplerState(&sampler, fSamplerState.ReleaseAndGetAddressOf()),
            "Could not create sampler state");

        OIV_D3D_SET_OBJECT_NAME(fSamplerState, "Sampler state");

        D3D11_BLEND_DESC blend;
        D3D11Utility::CreateD3D11DefaultBlendState(blend);
        
        // modify to alpha blend.
        blend.RenderTarget[0].BlendEnable = TRUE;
        blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;

        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateBlendState(&blend, fBlendState.ReleaseAndGetAddressOf()),
            "Could not create blend state");
    }

    void D3D11Renderer::CreateShaders()
    {
        using namespace std;
        using path = std::filesystem::path;

        path executableDirPath = LLUtils::PlatformUtility::GetExeFolder();
        path programsPath = executableDirPath / L"Resources/programs";

        fImageVertexShader = std::make_unique<D3D11VertexShader>(fDevice);
        fImageVertexShader->SetSourceFileName(programsPath / L"quad_vp.shader");
        fImageFragmentShader = std::make_unique< D3D11FragmentShader>(fDevice);
        fImageFragmentShader->SetSourceFileName(programsPath / L"quad_fp.shader");
        fSelectionFragmentShaer = std::make_unique<D3D11FragmentShader>(fDevice);
        fSelectionFragmentShaer->SetSourceFileName(programsPath / L"quad_selection_fp.shader");
        fImageSimpleFragmentShader = std::make_unique<D3D11FragmentShader>(fDevice);
        fImageSimpleFragmentShader->SetSourceFileName(programsPath / L"quad_simple_fp.shader");

        std::filesystem::path shaderCachePath = std::filesystem::path(fDataPath) / L"ShaderCache/.";
        
     

        D3D11Utility::LoadShader(fImageVertexShader, shaderCachePath);
        D3D11Utility::LoadShader(fImageFragmentShader, shaderCachePath);
        D3D11Utility::LoadShader(fSelectionFragmentShaer , shaderCachePath);
        D3D11Utility::LoadShader(fImageSimpleFragmentShader , shaderCachePath);

    }

    int D3D11Renderer::Init(const OIV_RendererInitializationParams& initParams)
    {
        fDataPath = initParams.dataPath;
        fDevice = std::make_shared<D3D11Device>();
        fDevice->Create(reinterpret_cast<HWND>(initParams.container));
        
        ResizeBackBuffer(1280, 800);
        CreateShaders();
        CreateBuffers();
        SetDevicestate();
        return 0;
    }

    void D3D11Renderer::UpdateViewportSize(int x, int y )
    {
        
        fViewport.Width = static_cast<FLOAT>(x);
        fViewport.Height = static_cast<FLOAT>(y); 
        fIsParamsDirty = true;

        ResizeBackBuffer(x, y);
        fDevice->GetContext()->RSSetViewports(1, &fViewport);
    }

    int D3D11Renderer::SetViewParams(const ViewParameters& viewParams)
    {
        UpdateViewportSize(static_cast<int>(viewParams.uViewportSize.x), static_cast<int>(viewParams.uViewportSize.y));
        CONSTANT_BUFFER_IMAGE_MAIN& buffer = fBufferImageMain->GetBuffer();
        
        buffer.uShowGrid = viewParams.showGrid;
        memcpy(buffer.transparencyColor1, viewParams.uTransparencyColor1.GetNormalizedColorValue<float>().data(), sizeof(float) * 4);
        memcpy(buffer.transparencyColor2, viewParams.uTransparencyColor2.GetNormalizedColorValue<float>().data(), sizeof(float) * 4);

        fIsParamsDirty = true;
        
        return 0;
    }

    void D3D11Renderer::UpdateGpuParameters()
    {
        if (fIsParamsDirty)
        {
            fBufferImageMain->Update();
            //fConstantBuffer->Write(sizeof(fShaderParameters), reinterpret_cast<const uint8_t*>(&fShaderParameters)
            //    , 0);
            fIsParamsDirty = false;
        }
    }

    void D3D11Renderer::DrawImage(const ImageEntry& entry)
    {
        auto renderable = entry.renderable;

        if (renderable->GetOpacity() == 0.0 || renderable->GetVisible() == false)
            return;

        //TODO: change to switch statement and unify constant buffres.

        renderable->PreRender();

        if (renderable->GetIsImageDirty() )
        {
            const_cast<ImageEntry&>(entry).texture = OIVD3DHelper::CreateTexture(fDevice, renderable->GetImage(), false);
            renderable->ClearImageDirty();
            
            if (entry.texture == nullptr)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "can not create texture");
        }
    
        CONSTANT_BUFFER_IMAGE_COMMON& gpuBuffer = fBufferImageCommon->GetBuffer();
        gpuBuffer.uImageOffset[0] = static_cast<float>(renderable->GetPosition().x);
        gpuBuffer.uImageOffset[1] = static_cast<float>(renderable->GetPosition().y);
        gpuBuffer.uvViewportSize[0] = static_cast<float>(fViewport.Width);
        gpuBuffer.uvViewportSize[1] = static_cast<float>(fViewport.Height);
        gpuBuffer.uvViewportSize[2] = static_cast<float>(1.0 / fViewport.Width);
        gpuBuffer.uvViewportSize[3] = static_cast<float>(1.0 / fViewport.Height);
        gpuBuffer.uImageSize[0] = static_cast<float>(entry.texture->GetCreateParams().width);
        gpuBuffer.uImageSize[1] = static_cast<float>(entry.texture->GetCreateParams().height);
        gpuBuffer.uScale[0] = static_cast<float>(renderable->GetScale().x);
        gpuBuffer.uScale[1] = static_cast<float>(renderable->GetScale().y);
        gpuBuffer.opacity = static_cast<float>(renderable->GetOpacity());

        fBufferImageCommon->Update();
        fBufferImageCommon->Use(ShaderStage::FragmentShader, 0);


        switch (renderable->GetImageRenderMode())
        {
        case OIV_Image_Render_mode::IRM_Overlay:
            fImageSimpleFragmentShader->Use();
            break;
        case  OIV_Image_Render_mode::IRM_MainImage:
            fBufferImageMain->Use(ShaderStage::FragmentShader, 1);
            fImageFragmentShader->Use();
            break;
        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }

        SetFilterLevel(renderable->GetFilterType());
        entry.texture->Use();
        fDevice->GetContext()->Draw(4, 0);
    }


    int D3D11Renderer::Redraw()
    {
        UpdateGpuParameters();
        ID3D11DeviceContext* context = fDevice->GetContext();
        
        //Draw images
        for (const MapImageEntry::value_type& idEntryPair : fImageEntries)
        {
            const ImageEntry& entry = idEntryPair.second;
            if (entry.renderable->GetImageRenderMode()  == OIV_Image_Render_mode::IRM_MainImage)
                DrawImage(entry);
        }
        
        //Draw selection rect.
        if (fBufferSelection->GetBuffer().uSelectionRect[0] != -1)
        {
            fBufferSelection->Use(ShaderStage::FragmentShader, 0);
            fSelectionFragmentShaer->Use();
            context->Draw(4, 0);
        }

        //Draw Overlays
        for (const MapImageEntry::value_type& idEntryPair : fImageEntries)
        {
            const ImageEntry& entry = idEntryPair.second;
            if (entry.renderable->GetImageRenderMode() == OIV_Image_Render_mode::IRM_Overlay)
                DrawImage(entry);
        }

        fDevice->GetSwapChain()->Present(0, 0);
        return 0;
    }

    int D3D11Renderer::SetFilterLevel(OIV_Filter_type filterType)
    {
        D3D11_SAMPLER_DESC desc;
        D3D11Utility::CreateD3D11DefaultSamplerState(desc);

        switch (filterType)
        {
        case FT_None:
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            break;
        case FT_Linear:
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            break;
        case FT_Lanczos3:
            desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
            break;
        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;

        }
        fDevice->GetdDevice()->CreateSamplerState(&desc, fSamplerState.ReleaseAndGetAddressOf());
        fDevice-> GetContext()->PSSetSamplers(static_cast<UINT>(0), static_cast<UINT>(1), fSamplerState.GetAddressOf());

        return 0;
    }

    int D3D11Renderer::SetselectionRect(const VisualSelectionRect& selection_rect)
    {
        fSelectionRect = selection_rect;
        CONSTANT_BUFFER_SELECTION_RECT& buffer = fBufferSelection->GetBuffer();
        
        buffer.uvViewportSize[0] = static_cast<int32_t>(fViewport.Width);
        buffer.uvViewportSize[1] = static_cast<int32_t>(fViewport.Height);
        buffer.uSelectionRect[0] = fSelectionRect.p0.x;
        buffer.uSelectionRect[1] = fSelectionRect.p0.y;
        buffer.uSelectionRect[2] = fSelectionRect.p1.x;
        buffer.uSelectionRect[3] = fSelectionRect.p1.y;
        fBufferSelection->Update();
        
        return 0;
    }

    int D3D11Renderer::SetExposure(const OIV_CMD_ColorExposure_Request& exposure)
    {
        CONSTANT_BUFFER_IMAGE_MAIN& buffer = fBufferImageMain->GetBuffer();
        buffer.exposure = static_cast<float>(exposure.exposure);
        buffer.offset = static_cast<float>(exposure.offset);
        buffer.gamma = static_cast<float>(exposure.gamma);
        buffer.saturation = static_cast<float>(exposure.saturation);
        
        fIsParamsDirty = true;
        return 0;
        
    }

    int D3D11Renderer::AddRenderable(IRenderable* renderable)
    {
        auto it = fImageEntries.find(renderable);
        if (it != fImageEntries.end())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "same image found");

        it = fImageEntries.emplace(renderable, ImageEntry{ nullptr,renderable }).first;
        
        return 0;
    }

    int D3D11Renderer::RemoveRenderable(IRenderable* renderable)
    {
        if (fImageEntries.erase(renderable) == 0)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::DuplicateItem, "can not remove image");

        return 0;
    }
}