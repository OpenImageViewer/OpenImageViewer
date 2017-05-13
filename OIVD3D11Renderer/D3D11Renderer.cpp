
#include <filesystem>
#include <d3dcommon.h>
#include <d3d11.h>
//#include <Utility.h>
#include <PlatformUtility.h>
#include <FileHelper.h>
#include "D3D11Renderer.h"
#include "D3D11Common.h"
#include "D3D11VertexShader.h"
#include "D3D11FragmentShader.h"


namespace OIV
{
 

    D3D11Renderer::D3D11Renderer()
    {
        fViewport.MinDepth = 0;
        fViewport.MaxDepth = 1;
        fShaderParameters.uvScale[0] = 10000;
        fShaderParameters.uvScale[1] = 10000;
    }

    void D3D11Renderer::SetDevicestate()
    {
        ID3D11DeviceContext* d3dContext = fDevice->GetContext();
        //Set GPU programs.
        fImageVertexShader->Use();
        fImageFragmentShader->Use();
      
        d3dContext->IASetInputLayout(fInputLayout);

        // Set fragment program constant buffer.
        d3dContext->PSSetConstantBuffers(0, 1, &fConstantBuffer);
        
        
        //Set quad vertex buffer
        UINT stride = 8;
        UINT offset = 0;
        d3dContext->IASetVertexBuffers(0, 1, &fVertexBuffer, &stride, &offset);
        d3dContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        
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

    void D3D11Renderer::CompileShaders()
    {
        using namespace std;
        wstring executableDirPath = LLUtils::PlatformUtility::GetExeFolder();
        wstring vertexShaderPath = executableDirPath + L"/Resources/programs/quad_vp.shader";
        wstring fragmentShaderPath = executableDirPath + L"/Resources/programs/quad_fp.shader";
        string vertexSource = LLUtils::File::ReadAllText(vertexShaderPath);
        string fragmentSource = LLUtils::File::ReadAllText(fragmentShaderPath);

        if (vertexSource.empty() == true || fragmentSource.empty() == true)
            D3D11Error::HandleError("Direct3D11 could not locate the GPU programs");

        fImageVertexShader->Load(vertexSource);
        fImageFragmentShader->Load(fragmentSource);
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

        // Fill in a buffer description.
        D3D11_BUFFER_DESC cbDesc;
        cbDesc.ByteWidth = sizeof(VS_CONSTANT_BUFFER) + (sizeof(VS_CONSTANT_BUFFER) % 16 == 0 ? 0 : 16 - sizeof(VS_CONSTANT_BUFFER) % 16);
        cbDesc.Usage = D3D11_USAGE_DYNAMIC;
        cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        cbDesc.MiscFlags = 0;
        cbDesc.StructureByteStride = 0;

        {
            // Fill in the subresource data.
            D3D11_SUBRESOURCE_DATA InitData;
            InitData.pSysMem = &fShaderParameters;
            InitData.SysMemPitch = 0;
            InitData.SysMemSlicePitch = 0;


            // Create the buffer.
            res = fDevice->GetdDevice()->CreateBuffer(&cbDesc, &InitData,
                &fConstantBuffer);

            D3D11Error::HandleDeviceError(res, "Can not create constant buffer");


        }

        D3D11_SAMPLER_DESC sampler;
        CreateDefaultSamplerState(sampler);
        
        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateSamplerState(&sampler, &fSamplerState),
            "Could not create sampler state");
        
        
        
    }


    void D3D11Renderer::CreateDefaultSamplerState(D3D11_SAMPLER_DESC &sampler)
    {
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MinLOD = -FLT_MAX;
        sampler.MaxLOD = FLT_MAX;
        sampler.MipLODBias = 0.0f;
        sampler.MaxAnisotropy = 1;
        sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
        sampler.BorderColor[0] = sampler.BorderColor[1] = sampler.BorderColor[2] = sampler.BorderColor[3] = 1.0f;
    }

    void D3D11Renderer::CreateShaders()
    {
        fImageVertexShader = new D3D11VertexShader(fDevice);
        fImageFragmentShader = new D3D11FragmentShader(fDevice);

        if (LoadShadersFromDisk() == false)
        {
            CompileShaders();
            SaveShadersToDisk();
        }
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
        fShaderParameters.uViewportSize[0] = fViewport.Width = static_cast<FLOAT>(x);
        fShaderParameters.uViewportSize[1] = fViewport.Height = static_cast<FLOAT>(y);
        fIsParamsDirty = true;

        ResizeBackBuffer(x, y);
        fDevice->GetContext()->RSSetViewports(1, &fViewport);
    }

    int D3D11Renderer::SetViewParams(const ViewParameters & viewParams)
    {
        UpdateViewportSize(static_cast<int>(viewParams.uViewportSize.x), static_cast<int>(viewParams.uViewportSize.y));
        fShaderParameters.uShowGrid = viewParams.showGrid;
        fShaderParameters.uvScale[0] =  static_cast<float>(viewParams.uvscale.x);
        fShaderParameters.uvScale[1] =  static_cast<float>(viewParams.uvscale.y);
        fShaderParameters.uvOffset[0] = static_cast<float>(viewParams.uvOffset.x);
        fShaderParameters.uvOffset[1] = static_cast<float>(viewParams.uvOffset.y);

        fIsParamsDirty = true;
        
        return 0;
    }

    void D3D11Renderer::UpdateGpuParameters()
    {
        if (fIsParamsDirty)
        {
            D3D11_MAPPED_SUBRESOURCE mapped;
            HRESULT res = fDevice->GetContext()->Map(fConstantBuffer, static_cast<UINT>(0), D3D11_MAP_WRITE_DISCARD, static_cast<UINT>(0), &mapped);
            D3D11Error::HandleDeviceError(res, "Can not map constant buffer");
            memcpy(mapped.pData, &fShaderParameters, sizeof(VS_CONSTANT_BUFFER));
            fDevice->GetContext()->Unmap(fConstantBuffer, static_cast<UINT>(0));
            fIsParamsDirty = false;
        }
    }


    int D3D11Renderer::Redraw()
    {
        UpdateGpuParameters();
        
        fDevice->GetContext()->Draw(4, 0);
        fDevice->GetSwapChain()->Present(0, 0);
        return 0;
    }

    int D3D11Renderer::SetFilterLevel(OIV_Filter_type filterType)
    {
        D3D11_SAMPLER_DESC desc;
        CreateDefaultSamplerState(desc);

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

        fShaderParameters.uImageSize[0] = static_cast<float>(image->GetWidth());
        fShaderParameters.uImageSize[1] = static_cast<float>(image->GetHeight());
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

    void D3D11Renderer::Destroy()
    {
        // Destory image.
        SAFE_RELEASE(fTextureShaderResourceView);
        SAFE_RELEASE(fTexture);
        SAFE_RELEASE(fSamplerState);

        //Destroy shaders

        if (fImageVertexShader != nullptr)
            delete fImageVertexShader;

        if (fImageFragmentShader != nullptr)
            delete fImageFragmentShader;

        // Destory buffers
        SAFE_RELEASE(fConstantBuffer);
        SAFE_RELEASE(fInputLayout);
        SAFE_RELEASE(fVertexBuffer);

        // Destroy swap chain
        SAFE_RELEASE(fRenderTargetView);
    
    }

    D3D11Renderer::~D3D11Renderer()
    {
        Destroy();
    }

    bool D3D11Renderer::LoadShadersFromDisk()
    {
        std::wstring oivAppDataFolder = LLUtils::PlatformUtility::GetAppDataFolder();

        std::experimental::filesystem::path p = oivAppDataFolder;
        std::experimental::filesystem::path vertexShader = p / L"vertexShader.bin";
        std::experimental::filesystem::path fragmentShader = p / L"fragmentShader.bin";
        try
        {

            BlobSharedPtr blob = BlobSharedPtr(new Blob());
            LLUtils::File::ReadAllBytes(vertexShader, blob->size, blob->buffer);
            fImageVertexShader->Load(blob);
            BlobSharedPtr blob1 = BlobSharedPtr(new Blob());

            LLUtils::File::ReadAllBytes(fragmentShader, blob1->size, blob1->buffer);
            fImageFragmentShader->Load(blob1);
            

            return true;
        }
        catch (...)
        { }

        return false;
    }

    void D3D11Renderer::SaveShadersToDisk()
    {
        std::wstring oivAppDataFolder = LLUtils::PlatformUtility::GetAppDataFolder();
        
        std::experimental::filesystem::path p = oivAppDataFolder;
        std::experimental::filesystem::path vertexShader = p / L"vertexShader.bin";
        std::experimental::filesystem::path fragmentShader = p / L"fragmentShader.bin";
        BlobSharedPtr blob = fImageVertexShader->GetShaderData();
        LLUtils::File::WriteAllBytes(vertexShader, blob->size, blob->buffer);
        blob = fImageFragmentShader->GetShaderData();
        LLUtils::File::WriteAllBytes(fragmentShader, blob->size, blob->buffer);
        
    }
}
