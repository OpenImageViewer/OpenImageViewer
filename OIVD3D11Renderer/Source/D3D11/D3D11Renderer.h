#pragma once

#include <d3d11.h>
#include <Image.h>
#include "D3D11Device.h"
#include "D3D11Shader.h"
#include "D3D11Buffer.h"
#include "D3D11Texture.h"
#include <API/defs.h>
#include <interfaces/IrendererDefs.h>

namespace OIV
{
    const std::string BLANK_STRING;

#pragma pack(1)
    struct VS_CONSTANT_BUFFER_SELECTIONRECT
    {
        int32_t uvViewportSize [4];
        int32_t uSelectionRect[4]; // p0 (x,y) ,  p1 (z,w)
    };

    

    struct VS_CONSTANT_BUFFER
    {
        float uvScale[2];
        float uvOffset[2];
        float uImageSize[2];
        float uViewportSize[2];
        int32_t uShowGrid;
        float exposure;
        float offset;
        float gamma;
    };
#pragma pack()


    struct ImageEntry
    {
        D3D11TextureSharedPtr texture;
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
        int SetselectionRect(const SelectionRect& selection_rect);
        int SetExposure(const OIV_CMD_ColorExposure_Request& exposure);


#pragma region //**** Private methods*****/
    private: 
        void ResizeBackBuffer(int x, int y);
        void CreateD3D11DefaultSamplerState(D3D11_SAMPLER_DESC& sampler);
        void CreateShaders();

        void CreateD3D11DefaultBlendState(D3D11_BLEND_DESC& blend);
        void CreateBuffers();
        void renderOneFrame();
        void UpdateViewportSize(int x, int y);
        void Destroy();
        void SetDevicestate();
#pragma endregion
    private:
        D3D11DeviceSharedPtr fDevice;
        
        D3D11ShaderUniquePtr fImageVertexShader;
        D3D11ShaderUniquePtr fImageFragmentShader;
        D3D11ShaderUniquePtr fSelectionFragmentShaer;
        bool fIsParamsDirty = true;
        SelectionRect fSelectionRect;
        std::vector<ImageEntry> fImageEntries = std::vector<ImageEntry>(100);

#pragma region /* Direct3D111 resources*/
        D3D11_VIEWPORT fViewport = {0};
        D3D11BufferBoundUniquePtr<VS_CONSTANT_BUFFER_SELECTIONRECT> fBufferSelection;
        D3D11BufferBoundUniquePtr<VS_CONSTANT_BUFFER> fConstantBuffer;

        ID3D11Buffer* fVertexBuffer = nullptr;
        ID3D11InputLayout* fInputLayout = nullptr;
        ID3D11RenderTargetView* fRenderTargetView = nullptr;
        ID3D11SamplerState*  fSamplerState = nullptr;
        ID3D11BlendState* fBlendState = nullptr;

#pragma endregion 
    };
}
