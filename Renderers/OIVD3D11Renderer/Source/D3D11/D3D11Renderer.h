#pragma once

#include <Image.h>
#include "D3D11Device.h"
#include "D3D11Shader.h"
#include "D3D11Buffer.h"
#include "D3D11Texture.h"
#include <RendererCore/RenderTypes.h>
#include <RendererCore/ViewParameters.h>
#include <RendererCore/IRenderBackend.h>
#include <LLUtils/Color.h>
#include <map>

namespace OIV
{
#pragma pack(1)
    // Shader ABI data with layout checked or preserved by the existing backend.
    struct CONSTANT_BUFFER_SELECTION_RECT
    {
        // Client viewport extent and existing shader reciprocals/padding.
        int32_t uvViewportSize[4];
        // Selection endpoints in client pixels.
        int32_t uSelectionRect[4];  // p0 (x,y) ,  p1 (z,w)
    };

    // Shader ABI data with layout checked or preserved by the existing backend.

    struct CONSTANT_BUFFER_GLOBALS
    {
        // First RGBA background color for the existing background pattern.
        float backgroundColor1[4];
        // Second RGBA background color for the existing background pattern.
        float backgroundColor2[4];
    };

    // Shader ABI data with layout checked or preserved by the existing backend.

    struct CONSTANT_BUFFER_IMAGE_COMMON
    {
        // Client viewport extent and existing shader reciprocals/padding.
        float uvViewportSize[4];
        // Texture extent in texels.
        float uImageSize[2];
        // Client placement in pixels.
        float uImageOffset[2];
        // Client pixels per texture texel along each axis.
        float uScale[2];
        // Presentation opacity multiplier.
        float opacity;
    };

    // Shader ABI data with layout checked or preserved by the existing backend.

    struct CONSTANT_BUFFER_IMAGE_MAIN
    {
        //------------------------
        // First RGBA transparency-checker color.
        float transparencyColor1[4];
        //------------------------
        // Second RGBA transparency-checker color.
        float transparencyColor2[4];
        //------------------------
        // Whether the texel grid is drawn.
        int32_t uShowGrid;
        // Existing color exposure multiplier.
        float exposure;
        // Existing color offset applied by the shader.
        float offset;
        // Existing gamma correction value.
        float gamma;
        //------------------------
        // Existing color saturation multiplier.
        float saturation;

        // Padding reserved by the existing shader ABI.

        float reserved[3];
        //------------------------
    };
#pragma pack()

    // GPU cache keyed by renderer-local ID, with no borrowed record pointers.
    struct ImageEntry
    {
        // Owned lazy GPU texture, absent until a visible nonempty record uploads.
        D3D11TextureSharedPtr texture;
        // Revision successfully uploaded to the current texture.
        uint64_t uploadedRevision = 0;
    };

    // Direct3D graphics implementation with lazy textures keyed by common registration ID.
    class D3D11Renderer final : public IRenderBackend
    {
      public:

        D3D11Renderer() = default;

        void Init(const OIV::RendererInitialization& initParams) override;
        void SetViewParams(const ViewParameters& viewParams) override;
        void Render(const RenderFrame& frame) override;
        void SetFilterLevel(OIV::FilterType filterType) override;
        void SetSelectionRect(VisualSelectionRect selectionRect) override;
        void SetExposure(const OIV::ColorExposure& exposure) override;
        void ReleaseImageResources(uint64_t id) noexcept override;
        void SetBackgroundColor(int index, LLUtils::Color backgroundColor) override;
        const RendererAdapter& GetSelectedAdapter() const override
        {
            return fDevice ? fDevice->GetSelectedAdapter() : IRenderBackend::GetSelectedAdapter();
        }
        std::vector<RendererAdapter> EnumerateAdapters() override { return D3D11Device::EnumerateAdapters(); }

#pragma region  //**** Private methods*****/

      private:

        void UpdateGpuParameters();
        void ResizeBackBuffer(int x, int y);
        void CreateShaders();
        void CreateBuffers();
        void UpdateViewportSize(int x, int y);
        void SetDeviceState();
        void DrawImage(ImageEntry& entry, const RenderImageRecord& record);
#pragma endregion

      private:

        // Graphics device used by this backend resource cache.
        D3D11DeviceSharedPtr fDevice;
        // Owned image vertex shader.
        D3D11ShaderUniquePtr fImageVertexShader;
        // Owned main-image fragment shader.
        D3D11ShaderUniquePtr fImageFragmentShader;
        // Owned selection fragment shader.
        D3D11ShaderUniquePtr fSelectionFragmentShader;
        // Owned overlay fragment shader.
        D3D11ShaderUniquePtr fImageSimpleFragmentShader;
        // Whether the existing main-image constant buffer requires an update.
        bool fIsParamsDirty = true;
        // Whether the existing global constant buffer requires an update.
        bool fGlobalsDirty = true;
        // Current client selection rectangle, with the existing absent sentinel.
        VisualSelectionRect fSelectionRect;
        // Existing RGBA background-pattern colors.
        std::array<LLUtils::ColorF32, 2> fBackgroundColors = {{
            {static_cast<uint8_t>(0), static_cast<uint8_t>(0), static_cast<uint8_t>(0), static_cast<uint8_t>(255)}
            // black
            ,
            {static_cast<uint8_t>(0), static_cast<uint8_t>(0), static_cast<uint8_t>(40),
             static_cast<uint8_t>(255)}  // dark blue
        }};

        using MapImageEntry = std::map<uint64_t, ImageEntry>;
        // Renderer-local GPU cache with successful upload revisions and retirement ownership.
        MapImageEntry fImageEntries;
        // Backend cache directory supplied by application composition. Empty disables persistent caching.
        std::filesystem::path fCacheDirectory;
        // Opaque canvas clear color.
        LLUtils::ColorF32 fBackgroundColor = {static_cast<uint8_t>(45), static_cast<uint8_t>(45),
                                              static_cast<uint8_t>(48), static_cast<uint8_t>(255)};

#pragma region /* Direct3D111 resources*/
        // Current Direct3D viewport.
        D3D11_VIEWPORT fViewport{.MinDepth = 0, .MaxDepth = 1};
        // Owned selection constants and their GPU buffer.
        D3D11BufferBoundUniquePtr<CONSTANT_BUFFER_SELECTION_RECT> fBufferSelection;
        // Owned global constants and their GPU buffer.
        D3D11BufferBoundUniquePtr<CONSTANT_BUFFER_GLOBALS> fBufferGlobals;
        // Owned per-image constants and their GPU buffer.
        D3D11BufferBoundUniquePtr<CONSTANT_BUFFER_IMAGE_COMMON> fBufferImageCommon;
        // Owned main-image constants and their GPU buffer.
        D3D11BufferBoundUniquePtr<CONSTANT_BUFFER_IMAGE_MAIN> fBufferImageMain;

        // Owned Direct3D sampling state.
        ComPtr<ID3D11SamplerState> fSamplerState;
        // Owned Direct3D image blending state.
        ComPtr<ID3D11BlendState> fBlendState;
        // Owned Direct3D quad vertex-input layout.
        ComPtr<ID3D11InputLayout> fInputLayout;
        // Owned Direct3D quad vertex buffer.
        ComPtr<ID3D11Buffer> fVertexBuffer;
        // Owned swap-chain render-target view.
        ComPtr<ID3D11RenderTargetView> fRenderTargetView;
#pragma endregion
    };
}  // namespace OIV
