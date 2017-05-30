#include <filesystem>
#include <d3dcommon.h>
#include <d3d11.h>
#include <PlatformUtility.h>
#include <FileHelper.h>
#include "D3D11Renderer.h"
#include "D3D11Common.h"
#include "D3D11VertexShader.h"
#include "D3D11FragmentShader.h"
#include "D3D11Utility.h"

namespace OIV
{
    D3D11Renderer::D3D11Renderer()
    {
        fViewport.MinDepth = 0;
        fViewport.MaxDepth = 1;
        /*fShaderParameters.uvScale[0] = 10000;
        fShaderParameters.uvScale[1] = 10000;*/
    }

    void D3D11Renderer::SetDevicestate()
    {
        ID3D11DeviceContext* d3dContext = fDevice->GetContext();
        //Set GPU programs.
        fImageVertexShader->Use();
        fImageFragmentShader->Use();
      
        d3dContext->IASetInputLayout(fInputLayout);

        //Set quad vertex buffer
        UINT stride = 8;
        UINT offset = 0;
        d3dContext->IASetVertexBuffers(0, 1, &fVertexBuffer, &stride, &offset);
        d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        float white[4] = { 1.0f,1.0f,1.0f,1.0f };

        d3dContext->OMSetBlendState(fBlendState,white, static_cast<UINT>(0xFFFFFFFF));
        d3dContext->PSSetSamplers(static_cast<UINT>(0), static_cast<UINT>(1), &fSamplerState);
        
        d3dContext->RSSetViewports(1, &fViewport);
    }

    void D3D11Renderer::ResizeBackBuffer(int x, int y)
    {
        IDXGISwapChain* d3dSwapChain = fDevice->GetSwapChain();
        ID3D11DeviceContext* d3dContext = fDevice->GetContext();
        ID3D11Device* d3dDevice = fDevice->GetdDevice();
      

        SAFE_RELEASE(fRenderTargetView);
        d3dSwapChain->ResizeBuffers(1, x, y, DXGI_FORMAT_UNKNOWN, static_cast<UINT>(0));
        ID3D11Texture2D* backbuffer = nullptr;
        d3dSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backbuffer));
        // create a render target pointing to the back buffer
        d3dDevice->CreateRenderTargetView(backbuffer, nullptr, &fRenderTargetView);
        SAFE_RELEASE(backbuffer);
        FLOAT white[4] = { 1,1,1,1 };
        d3dContext->OMSetRenderTargets(1, &fRenderTargetView, nullptr);
        d3dContext->ClearRenderTargetView(fRenderTargetView, white);
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
        HRESULT res = fDevice->GetdDevice()->CreateBuffer(&bufferDesc, &InitData, &fVertexBuffer);

        D3D11Error::HandleDeviceError(res, "Could not create buffer");

        // create buffer input layout

        D3D11_INPUT_ELEMENT_DESC ied[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

    D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateInputLayout(ied
        , 1
            , fImageVertexShader->GetShaderData()->buffer
            , fImageVertexShader->GetShaderData()->size, &fInputLayout)
    , "Could not crate Input layout");

        //******** Create constant buffer *********/

    {
        // Fill in a buffer description.
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = sizeof(VS_CONSTANT_BUFFER) + (sizeof(VS_CONSTANT_BUFFER) % 16 == 0 ? 0 : 16 - sizeof(VS_CONSTANT_BUFFER) % 16);
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

        fConstantBuffer = D3D11BufferBoundUniquePtr<VS_CONSTANT_BUFFER>(new D3D11BufferBound<VS_CONSTANT_BUFFER>
            (fDevice, cbDesc, nullptr));

    }


            //Create constant buffer for selection rect.
    {
        // Fill in a buffer description.
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = sizeof(VS_CONSTANT_BUFFER_SELECTIONRECT);
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

        fBufferSelection = D3D11BufferBoundUniquePtr<VS_CONSTANT_BUFFER_SELECTIONRECT> (
            new D3D11BufferBound<VS_CONSTANT_BUFFER_SELECTIONRECT>(fDevice, cbDesc, nullptr));

        // Mark cpu buffer as invalid selection rect.
        fBufferSelection->GetBuffer().uSelectionRect[0] = -1;
    }
        

        D3D11_SAMPLER_DESC sampler;
        D3D11Utility::CreateD3D11DefaultSamplerState(sampler);

        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateSamplerState(&sampler, &fSamplerState),
            "Could not create sampler state");

        D3D11_BLEND_DESC blend;
        D3D11Utility::CreateD3D11DefaultBlendState(blend);
        
        // modify to alpha blend.
        blend.RenderTarget[0].BlendEnable = TRUE;
        blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;

        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateBlendState(&blend, &fBlendState),
            "Could not create blend state");
    }

    void D3D11Renderer::CreateShaders()
    {
        fImageVertexShader = D3D11ShaderUniquePtr (new D3D11VertexShader(fDevice));
        fImageFragmentShader = D3D11ShaderUniquePtr(new D3D11FragmentShader(fDevice));
        fSelectionFragmentShaer = D3D11ShaderUniquePtr(new D3D11FragmentShader(fDevice));

        using namespace std;
        using path = std::experimental::filesystem::path;
        
        path executableDirPath = LLUtils::PlatformUtility::GetExeFolder();
        path programsPath = executableDirPath / L"/Resources/programs";
        D3D11Utility::LoadShader(fImageVertexShader, programsPath / L"quad_vp.shader");
        D3D11Utility::LoadShader(fImageFragmentShader, programsPath / L"quad_fp.shader");
        D3D11Utility::LoadShader(fSelectionFragmentShaer, programsPath / L"quad_selection_fp.shader");
    }

    int D3D11Renderer::Init(std::size_t container)
    {
        fDevice = D3D11DeviceSharedPtr(new D3D11Device());
        fDevice->Create(reinterpret_cast<HWND>(container));
        
        ResizeBackBuffer(1280, 800);
        CreateShaders();
        CreateBuffers();
        SetDevicestate();
        return 0;
    }

    void D3D11Renderer::UpdateViewportSize(int x, int y )
    {
        
        fConstantBuffer->GetBuffer().uViewportSize[0] = fViewport.Width = static_cast<FLOAT>(x);
        fConstantBuffer->GetBuffer().uViewportSize[1] = fViewport.Height = static_cast<FLOAT>(y);
        fIsParamsDirty = true;

        ResizeBackBuffer(x, y);
        fDevice->GetContext()->RSSetViewports(1, &fViewport);
    }

    int D3D11Renderer::SetViewParams(const ViewParameters & viewParams)
    {
        UpdateViewportSize(static_cast<int>(viewParams.uViewportSize.x), static_cast<int>(viewParams.uViewportSize.y));
        VS_CONSTANT_BUFFER& buffer = fConstantBuffer->GetBuffer();
        
        buffer.uShowGrid = viewParams.showGrid;
        buffer.uvScale[0] =  static_cast<float>(viewParams.uvscale.x);
        buffer.uvScale[1] =  static_cast<float>(viewParams.uvscale.y);
        buffer.uvOffset[0] = static_cast<float>(viewParams.uvOffset.x);
        buffer.uvOffset[1] = static_cast<float>(viewParams.uvOffset.y);
        
        //fConstantBuffer->Update();
        fIsParamsDirty = true;
        
        return 0;
    }

    void D3D11Renderer::UpdateGpuParameters()
    {
        if (fIsParamsDirty)
        {
            fConstantBuffer->Update();
            //fConstantBuffer->Write(sizeof(fShaderParameters), reinterpret_cast<const uint8_t*>(&fShaderParameters)
            //    , 0);
            fIsParamsDirty = false;
        }
    }


    int D3D11Renderer::Redraw()
    {
        UpdateGpuParameters();
        ID3D11DeviceContext* context = fDevice->GetContext();

        //Draw image.
        fConstantBuffer->Use(ShaderStage::SS_FragmentShader);
        
        fImageFragmentShader->Use();
        context->Draw(4, 0);

        //Draw selection rect.
        if (fBufferSelection->GetBuffer().uSelectionRect[0] != -1)
        {
            fBufferSelection->Use(ShaderStage::SS_FragmentShader);
            fSelectionFragmentShaer->Use();
            context->Draw(4, 0);
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
            throw std::runtime_error("wrong or corrupted value");

        }

        SAFE_RELEASE(fSamplerState);
        fDevice->GetdDevice()->CreateSamplerState(&desc, &fSamplerState);
        fDevice-> GetContext()->PSSetSamplers(static_cast<UINT>(0), static_cast<UINT>(1), &fSamplerState);

        return 0;
    }

    int D3D11Renderer::SetImage(const IMCodec::ImageSharedPtr image)
    {
        if (image->GetImageType() != IMCodec::TF_I_R8_G8_B8_A8)
            D3D11Error::HandleError("Direct3D11 renderer supports only RGBA pixel format");

        DXGI_FORMAT textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        
        fConstantBuffer->GetBuffer().uImageSize[0] = static_cast<float>(image->GetWidth());
        fConstantBuffer->GetBuffer().uImageSize[1] = static_cast<float>(image->GetHeight());
        fIsParamsDirty = true;
        

        SAFE_RELEASE(fTextureShaderResourceView);
        SAFE_RELEASE(fTexture);

        D3D11_TEXTURE2D_DESC desc = { 0 };
        desc.Width = static_cast<UINT>(image->GetWidth());
        desc.Height = static_cast<UINT>(image->GetHeight());
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = textureFormat;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = static_cast<UINT>(0);
        desc.MiscFlags = 0;
        const D3D11_SUBRESOURCE_DATA subResourceData = {
                                                      (image->GetBuffer())
                                                    , static_cast<UINT>(image->GetRowPitchInBytes())
                                                    , static_cast<UINT>(0) };

        
        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateTexture2D(&desc, &subResourceData, &fTexture)
            , "Can not create texture");

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
        memset(&viewDesc, 0, sizeof viewDesc);

        viewDesc.Format = textureFormat;
        viewDesc.Texture2D.MostDetailedMip = 0;
        viewDesc.Texture2D.MipLevels = 1;
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

        

        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateShaderResourceView(fTexture, &viewDesc, &fTextureShaderResourceView)
            , "Can not create 'Shader resource view'");
        
        fDevice->GetContext()->PSSetShaderResources(static_cast<UINT>(0), static_cast<UINT>(1), &fTextureShaderResourceView);

        return 0;
    }

    int D3D11Renderer::SetselectionRect(const SelectionRect& selection_rect)
    {
        fSelectionRect = selection_rect;
        VS_CONSTANT_BUFFER_SELECTIONRECT& buffer = fBufferSelection->GetBuffer();
        
        buffer.uvViewportSize[0] = static_cast<int32_t>(fConstantBuffer->GetBuffer().uViewportSize[0]);
        buffer.uvViewportSize[1] = static_cast<int32_t>(fConstantBuffer->GetBuffer().uViewportSize[1]);
        buffer.uSelectionRect[0] = fSelectionRect.p0.x;
        buffer.uSelectionRect[1] = fSelectionRect.p0.y;
        buffer.uSelectionRect[2] = fSelectionRect.p1.x;
        buffer.uSelectionRect[3] = fSelectionRect.p1.y;
        fBufferSelection->Update();
    
      
        Redraw();
        return 0;
    }

    void D3D11Renderer::Destroy()
    {
        // Destory image.
        SAFE_RELEASE(fTextureShaderResourceView);
        SAFE_RELEASE(fTexture);
        SAFE_RELEASE(fSamplerState);

        SAFE_RELEASE(fBlendState);
        
        SAFE_RELEASE(fInputLayout);
        SAFE_RELEASE(fVertexBuffer);

        // Destroy swap chain
        SAFE_RELEASE(fRenderTargetView);
    
    }

    D3D11Renderer::~D3D11Renderer()
    {
        Destroy();
    }
}