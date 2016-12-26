#include "D3D11Renderer.h"
#include <d3dcommon.h>
#include <d3d10sdklayers.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdint.h>
#include "../OIVUtil/Utility.h"
#include "../OIVUtil/FileHelper.h"
#include <filesystem>


namespace OIV
{
 
#define SAFE_RELEASE(x) {if (x) {x->Release(); x = nullptr; } }

    D3D11Renderer::D3D11Renderer()
    {
        fViewport.MinDepth = 0;
        fViewport.MaxDepth = 1;
        fShaderParameters.uvScale[0] = 10000;
        fShaderParameters.uvScale[1] = 10000;
        fShowDeviceErrors = false;
    }

    void D3D11Renderer::HandleCompileError(ID3DBlob* errors) const
    {
        using namespace std;
        if (errors == nullptr)
            HandleError("Direct3D11 raised a logic error.\nReason: misuse of error handling - no error");

        size_t size = errors->GetBufferSize();
        unique_ptr<char> errorString = unique_ptr<char>(new char[size]);
        memcpy(errorString.get(), errors->GetBufferPointer(), size);
    
        string errorMessage = string("Direct3D11 Can not compile GPU program.\nreason: ") + errorString.get();
        
        HandleError(errorMessage);
    }

    void D3D11Renderer::SetDevicestate()
    {
        //Set GPU programs.
        d3dContext->VSSetShader(fVertexShader, nullptr, 0);
        d3dContext->PSSetShader(fFragmentShader, nullptr, 0);
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
        //Create shader/*
        wstring executableDirPath = Utility::GetExeFolder();
        
        wstring vertexShaderPath = executableDirPath   + L"/Resources/programs/quad_vp.shader";
        wstring fragmentShaderPath = executableDirPath + L"/Resources/programs/quad_fp.shader";
        string vertexSource = File::ReadAllText(vertexShaderPath);
        string fragmentSource = File::ReadAllText(fragmentShaderPath);

        if (vertexSource.empty() == true || fragmentSource.empty() == true)
            HandleError("Direct3D11 could not locate the GPU programs");

        D3D_SHADER_MACRO macros[2];
        macros[0].Definition = "1";
        macros[0].Name = "HLSL";

        macros[1].Definition = nullptr;
        macros[1].Name = nullptr;

        ID3DBlob* microCode = nullptr;
        ID3DBlob* errors = nullptr;
        HRESULT res;


        res =
            D3DCompile(
                static_cast<const void*>(vertexSource.c_str())
                , vertexSource.length()
                , nullptr /*source name*/
                , &macros[0]
                , nullptr
                , "main"
                , "vs_4_0"
                , D3DCOMPILE_OPTIMIZATION_LEVEL3
                , 0
                , &microCode
                , &errors
            );

        if (SUCCEEDED(res) == false)
            HandleCompileError(errors);

        fVertexShaderData = Blob(microCode);
        SAFE_RELEASE(microCode);
        SAFE_RELEASE(errors);
            
      


        res =
            D3DCompile(
                static_cast<const void*>(fragmentSource.c_str())
                , fragmentSource.length()
                , nullptr /*source name*/
                , &macros[0]
                , nullptr
                , "main"
                , "ps_4_0"
                , D3DCOMPILE_OPTIMIZATION_LEVEL3
                , 0
                , &microCode
                , &errors
            );

        if (SUCCEEDED(res) == false)
            HandleCompileError(errors);

        fFragmentShaderData =  Blob(microCode);
        SAFE_RELEASE(errors);
        SAFE_RELEASE(microCode);
        
    }

    void D3D11Renderer::CreateDevice(HWND windowHandle)
    {
        D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
        D3D_FEATURE_LEVEL obtainedLevel;


        DXGI_SWAP_CHAIN_DESC scd = { 0 };

        scd.BufferCount = 1;
        scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
        scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

        scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        scd.OutputWindow = windowHandle;
        scd.SampleDesc.Count = 1;
        scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
        scd.Windowed = true;
        scd.BufferDesc.Width = 1280;
        scd.BufferDesc.Height = 800;
        scd.BufferDesc.RefreshRate.Numerator = 0;
        scd.BufferDesc.RefreshRate.Denominator = 1;

        UINT createFlags = 0;
        createFlags |= D3D11_CREATE_DEVICE_SINGLETHREADED;

#ifdef _DEBUG
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif


        HRESULT res =
            D3D11CreateDeviceAndSwapChain(
                nullptr,
                D3D_DRIVER_TYPE_HARDWARE,
                nullptr,
                createFlags,
                requestedLevels,
                sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
                D3D11_SDK_VERSION,
                &scd,
                &d3dSwapChain,
                &d3dDevice,
                &obtainedLevel,
                &d3dContext);
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
        HRESULT res = d3dDevice->CreateBuffer(&bufferDesc, &InitData, &fVertexBuffer);

        HandleDeviceError(res, "Could not create buffer");

        // create buffer input layout

        D3D11_INPUT_ELEMENT_DESC ied[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

    HandleDeviceError(d3dDevice->CreateInputLayout(ied, 1, fVertexShaderData.buffer , fVertexShaderData.size, &fInputLayout)
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
            res = d3dDevice->CreateBuffer(&cbDesc, &InitData,
                &fConstantBuffer);

            HandleDeviceError(res, "Can not create constant buffer");


        }

        D3D11_SAMPLER_DESC sampler;
        CreateDefaultSamplerState(sampler);
        
        d3dDevice->CreateSamplerState(&sampler, &fSamplerState);
        
        
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
        if (LoadShadersFromDisk() == false)
        {
            CompileShaders();
            SaveShadersToDisk();
        }


        HandleDeviceError(d3dDevice->CreateVertexShader(fVertexShaderData.buffer, fVertexShaderData.size, nullptr, &fVertexShader)
            , " could not create vertex shader from microcode");

        HandleDeviceError(d3dDevice->CreatePixelShader(fFragmentShaderData.buffer, fFragmentShaderData.size, nullptr, &fFragmentShader)
            , " could not create pixel shader from microcode");
    }

    int D3D11Renderer::Init(size_t container)
    {
        CreateDevice(reinterpret_cast<HWND>(container));
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
        d3dContext->RSSetViewports(1, &fViewport);
    }

    void D3D11Renderer::HandleError(std::string errorMessage) const
    {
        if (fShowDeviceErrors)
            MessageBoxA(static_cast<HWND>(0), errorMessage.c_str(), "Error", MB_OK);
        else
            throw std::logic_error(errorMessage);
    }

    void D3D11Renderer::HandleDeviceError(HRESULT result, std::string&& errorMessage) const
    {
        HandleDeviceError(result, errorMessage);
    }

    void D3D11Renderer::HandleDeviceError(HRESULT result, const std::string& errorMessage ) const
    {
        if (SUCCEEDED(result) == false)
        {
            std::string message = "Direct3D11 could complete the operation.";
                if (errorMessage.empty() == false)
                    message += "\n" + errorMessage;
            HandleError(message);
        }
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
            HRESULT res = d3dContext->Map(fConstantBuffer, static_cast<UINT>(0), D3D11_MAP_WRITE_DISCARD, static_cast<UINT>(0), &mapped);
            HandleDeviceError(res, "Can not map constant buffer");
            memcpy(mapped.pData, &fShaderParameters, sizeof(VS_CONSTANT_BUFFER));
            d3dContext->Unmap(fConstantBuffer, static_cast<UINT>(0));
            fIsParamsDirty = false;
        }
    }


    int D3D11Renderer::Redraw()
    {
        UpdateGpuParameters();
        d3dContext->Draw(4, 0);
        d3dSwapChain->Present(0, 0);
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
        d3dDevice->CreateSamplerState(&desc, &fSamplerState);
        d3dContext->PSSetSamplers(static_cast<UINT>(0), static_cast<UINT>(1), &fSamplerState);

        return 0;
    }

    int D3D11Renderer::SetImage(const ImageSharedPtr image)
    {
        if (image->GetImageType() != IT_BYTE_RGBA)
            HandleError("Direct3D11 renderer supports only RGBA pixel format");

        fShaderParameters.uImageSize[0] = static_cast<float>(image->GetWidth());
        fShaderParameters.uImageSize[1] = static_cast<float>(image->GetHeight());
        fIsParamsDirty = true;
        

        SAFE_RELEASE(fTextureShaderResourceView);
        SAFE_RELEASE(fTexture);

        D3D11_TEXTURE2D_DESC desc = { 0 };
        desc.Width = static_cast<UINT>(image->GetWidth());
        desc.Height = static_cast<UINT>(image->GetHeight());
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = static_cast<UINT>(0);
        desc.MiscFlags = 0;
        const D3D11_SUBRESOURCE_DATA subResourceData = {
                                                      const_cast<void*>(image->GetBuffer())
                                                    , static_cast<UINT>(image->GetRowPitchInBytes())
                                                    , static_cast<UINT>(0) };

        

        HandleDeviceError(d3dDevice->CreateTexture2D(&desc, &subResourceData, &fTexture)
            , "Can not create texture");

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
        memset(&viewDesc, 0, sizeof viewDesc);

        viewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        viewDesc.Texture2D.MostDetailedMip = 0;
        viewDesc.Texture2D.MipLevels = 1;
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

        

        HandleDeviceError(d3dDevice->CreateShaderResourceView(fTexture, &viewDesc, &fTextureShaderResourceView)
            , "Can not create 'Shader resource view'");
        
        d3dContext->PSSetShaderResources(static_cast<UINT>(0), static_cast<UINT>(1), &fTextureShaderResourceView);

        return 0;
    }

    void D3D11Renderer::Destroy()
    {
        // Destory image.
        SAFE_RELEASE(fTextureShaderResourceView);
        SAFE_RELEASE(fTexture);
        SAFE_RELEASE(fSamplerState);

        // Destory GPU programs.
        SAFE_RELEASE(fVertexShader);
        SAFE_RELEASE(fFragmentShader);

        // Destory buffers
        SAFE_RELEASE(fConstantBuffer);
        SAFE_RELEASE(fInputLayout);
        SAFE_RELEASE(fVertexBuffer);

        // Destroy swap chain
        SAFE_RELEASE(fRenderTargetView);
        SAFE_RELEASE(d3dSwapChain);

        //Destroy device
        SAFE_RELEASE(d3dContext);
        SAFE_RELEASE(d3dDevice);
    }

    D3D11Renderer::~D3D11Renderer()
    {
        Destroy();
    }

    bool D3D11Renderer::LoadShadersFromDisk()
    {
        std::wstring oivAppDataFolder = Utility::GetAppDataFolder();

        std::experimental::filesystem::path p = oivAppDataFolder;
        std::experimental::filesystem::path vertexShader = p / L"vertexShader.bin";
        std::experimental::filesystem::path fragmentShader = p / L"fragmentShader.bin";
        try
        {
            File::ReadAllBytes(vertexShader, fVertexShaderData.size, fVertexShaderData.buffer);
            File::ReadAllBytes(fragmentShader, fFragmentShaderData.size, fFragmentShaderData.buffer);
            return true;
        }
        catch (...)
        { }

        return false;
    }

    void D3D11Renderer::SaveShadersToDisk()
    {
        std::wstring oivAppDataFolder = Utility::GetAppDataFolder();
        
        std::experimental::filesystem::path p = oivAppDataFolder;
        std::experimental::filesystem::path vertexShader = p / L"vertexShader.bin";
        std::experimental::filesystem::path fragmentShader = p / L"fragmentShader.bin";
        
        File::WriteAllBytes(vertexShader, fVertexShaderData.size, fVertexShaderData.buffer);
        File::WriteAllBytes(fragmentShader, fFragmentShaderData.size, fFragmentShaderData.buffer);
        
    }
}
