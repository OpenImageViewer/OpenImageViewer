#pragma once

#include <Image.h>
#include "D3D11Device.h"
#include "D3D11Shader.h"
#include "D3D11Buffer.h"
#include "D3D11Texture.h"
#include <defs.h>
#include <Interfaces/IRendererDefs.h>
#include <Interfaces/IRenderable.h>
#include <LLUtils/Color.h>
#include <map>

namespace OIV
{
#pragma pack(1)
    struct CONSTANT_BUFFER_SELECTION_RECT
    {
        int32_t uvViewportSize [4];
        int32_t uSelectionRect[4]; // p0 (x,y) ,  p1 (z,w)
    };

    struct CONSTANT_BUFFER_GLOBALS
    {
        float backgroundColor1[4];
        float backgroundColor2[4];
    };

    struct CONSTANT_BUFFER_IMAGE_COMMON
    {
        float uvViewportSize[4];
        float uImageSize[2];
        float uImageOffset[2];
        float uScale[2];
        float opacity;
    };

    struct CONSTANT_BUFFER_IMAGE_MAIN
    {
        //------------------------
        float transparencyColor1[4];
        //------------------------
        float transparencyColor2[4];
        //------------------------
        int32_t uShowGrid;
        float exposure;
        float offset;
        float gamma;
        //------------------------
        float saturation;

        float reserved[3];
        //------------------------
        
    };
#pragma pack()


    enum class ImageDisplayMode
    {
          Default
        , ImageView
        , Overlay
    };


    struct ImageEntry
    {
        D3D11TextureSharedPtr texture;
        IRenderable* renderable;
    };

    class D3D11Renderer 
    {
    
    public:
         D3D11Renderer();
    public:
        int Init(const OIV_RendererInitializationParams& initParams);
        int SetViewParams(const ViewParameters& viewParams);
        void UpdateGpuParameters();
        int Redraw();
        int SetFilterLevel(OIV_Filter_type filterType);
        int SetselectionRect(const VisualSelectionRect& selection_rect);
        int SetExposure(const OIV_CMD_ColorExposure_Request& exposure);
        int AddRenderable(IRenderable* renderable);
        int RemoveRenderable(IRenderable* renderable);
        int SetBackgroundColor(int index, LLUtils::Color backgroundColor);

#pragma region //**** Private methods*****/
    private: 
        void ResizeBackBuffer(int x, int y);
        void CreateShaders();
        void CreateBuffers();
        void UpdateViewportSize(int x, int y);
        void SetDevicestate();
        void DrawImage(const ImageEntry& entry);
#pragma endregion
    private:
        D3D11DeviceSharedPtr fDevice;
        D3D11ShaderUniquePtr fImageVertexShader;
        D3D11ShaderUniquePtr fImageFragmentShader;
        D3D11ShaderUniquePtr fSelectionFragmentShaer;
        D3D11ShaderUniquePtr fImageSimpleFragmentShader;
        bool fIsParamsDirty = true;
        bool fGlobalsDirty = true;
        VisualSelectionRect fSelectionRect;
        std::array<LLUtils::ColorF32, 2> fBackgroundColors =
        {
            {
                 {static_cast<uint8_t>(0), static_cast < uint8_t>(0),static_cast < uint8_t>(0), static_cast < uint8_t>(255)} // black
                ,{static_cast <uint8_t>(0),static_cast < uint8_t>(0),static_cast < uint8_t>(40),static_cast < uint8_t>(255)} // dark blue
            }
        };


        struct MapLess
        {
            bool operator() (const IRenderable* A, const  IRenderable* B) const
            {
                return A->GetID() < B->GetID();
            }
        };


        using MapImageEntry = std::map<IRenderable*, ImageEntry, MapLess>;
        MapImageEntry fImageEntries;
        OIVString fDataPath;
        LLUtils::ColorF32 fBackgroundColor = { static_cast<uint8_t>(45),static_cast < uint8_t>(45),static_cast < uint8_t>(48),static_cast < uint8_t>(255) };

#pragma region /* Direct3D111 resources*/
        D3D11_VIEWPORT fViewport {};
        D3D11BufferBoundUniquePtr<CONSTANT_BUFFER_SELECTION_RECT> fBufferSelection;
        D3D11BufferBoundUniquePtr<CONSTANT_BUFFER_GLOBALS> fBufferGlobals;
        D3D11BufferBoundUniquePtr<CONSTANT_BUFFER_IMAGE_COMMON> fBufferImageCommon;
        D3D11BufferBoundUniquePtr<CONSTANT_BUFFER_IMAGE_MAIN> fBufferImageMain;
        
        ComPtr<ID3D11SamplerState>  fSamplerState;
        ComPtr<ID3D11BlendState> fBlendState;
        ComPtr<ID3D11InputLayout> fInputLayout;
        ComPtr<ID3D11Buffer> fVertexBuffer;
        ComPtr<ID3D11RenderTargetView> fRenderTargetView;
#pragma endregion 
    };
}
