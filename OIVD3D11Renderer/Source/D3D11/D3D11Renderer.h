#pragma once

#include <d3d11.h>
#include <Image.h>
#include "D3D11Device.h"
#include "D3D11Shader.h"
#include "../../../OIVGLRenderer/OIVGLRendererFactory.h"

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



    class D3D11Renderer 
    {
    
     public:
        D3D11Renderer();
        ~D3D11Renderer();

    public:
        // TODO: remove IRenderer compliant methods by generalizing D3D11Renderer.
        int Init(std::size_t container);
        int SetViewParams(const ViewParameters& viewParams);
        void UpdateGpuParameters();
        int Redraw();
        int SetFilterLevel(OIV_Filter_type filterType);
        int SetImage(const IMCodec::ImageSharedPtr image);

  

 #pragma region //**** Private methods*****/
    private: 
        bool LoadShadersFromDisk();
        void SaveShadersToDisk();
        void ResizeBackBuffer(int x, int y);
        void CompileShaders();
        void CreateDefaultSamplerState(D3D11_SAMPLER_DESC& sampler);
        void CreateShaders();

        void CreateBuffers();
        void renderOneFrame();
        void UpdateViewportSize(int x, int y);
        void Destroy();
        void SetDevicestate();
#pragma endregion
    private:
        D3D11DeviceSharedPtr fDevice;
        D3D11Shader* fImageVertexShader;
        D3D11Shader* fImageFragmentShader;
        bool fIsParamsDirty = true;

#pragma region /* Direct3D111 resources*/
        VS_CONSTANT_BUFFER fShaderParameters = { 0 };
        D3D11_VIEWPORT fViewport = {0};
        ID3D11Buffer* fConstantBuffer = nullptr;
        ID3D11Texture2D *fTexture = nullptr;
        ID3D11Buffer* fVertexBuffer = nullptr;
        ID3D11InputLayout* fInputLayout = nullptr;
        ID3D11RenderTargetView* fRenderTargetView = nullptr;
        ID3D11ShaderResourceView* fTextureShaderResourceView = nullptr;
        ID3D11SamplerState*  fSamplerState = nullptr;
        
#pragma endregion 
    };
}
