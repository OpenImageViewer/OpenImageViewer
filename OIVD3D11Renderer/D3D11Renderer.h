#pragma once
#include "../oiv/interfaces/IRenderer.h"
#include <d3d11.h>

namespace OIV
{
    const std::string BLANK_STRING;
    struct VS_CONSTANT_BUFFER
    {
        float uvScale[2];
        float uvOffset[2];
        float uImageSize[2];
        float uViewportSize[2];
        int32_t uShowGrid;
    };

    class D3D11Renderer : public IRenderer
    {
    
     public:
        D3D11Renderer();
        ~D3D11Renderer();
        
 #pragma region /****IRenderer Overrides************/
    public:
        int Init(size_t container) override;
        int SetViewParams(const ViewParameters& viewParams) override;
        void UpdateGpuParameters();
        int Redraw() override;
        int SetFilterLevel(int filterLevel) override;
        int SetImage(const ImageSharedPtr image) override;
        
#pragma endregion

 #pragma region //**** Private methods*****/
    private: 
        void ResizeBackBuffer(int x, int y);
        void CreateShaders();
        void CreateDevice(HWND windowHandle);
        void CreateDefaultSamplerState(D3D11_SAMPLER_DESC& sampler);
        void CreateBuffers();
        void UpdateGpuParams();
        void renderOneFrame();
        void UpdateViewportSize(int x, int y);
        void ReCreateTexture(size_t width, size_t height);
        void PrepareResources();
        void Destroy();

        void HandleError(std::string errorMessage) const;
        void HandleDeviceError(HRESULT result, std::string errorMessage = BLANK_STRING) const;
        void HandleCompileError(ID3DBlob* errors) const;
        void SetDevicestate();
#pragma endregion
    private:
        bool fIsParamsDirty = true;
        bool fShowDeviceErrors = true;
#pragma region /* Direct3D111 resources*/
        VS_CONSTANT_BUFFER fShaderParameters = { 0 };
        D3D11_VIEWPORT fViewport = {0};
        ID3D11Buffer* fConstantBuffer = nullptr;
        ID3D11Device* d3dDevice = nullptr;
        ID3D11DeviceContext* d3dContext = nullptr;
        IDXGISwapChain* d3dSwapChain = nullptr;
        ID3D11Texture2D *fTexture = nullptr;
        ID3D11VertexShader* fVertexShader = nullptr;
        ID3DBlob* fVertexShaderMicroCode = nullptr;
        ID3D11PixelShader* fFragmentShader = nullptr;
        ID3DBlob* fFragmentShaderMicroCode = nullptr;
        ID3D11Buffer* fVertexBuffer = nullptr;
        ID3D11InputLayout* fInputLayout = nullptr;
        ID3D11RenderTargetView* fRenderTargetView = nullptr;
        ID3D11ShaderResourceView* fTextureShaderResourceView = nullptr;
        ID3D11SamplerState*  fSamplerState = nullptr;
        
#pragma endregion 
    };
}