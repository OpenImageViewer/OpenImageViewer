#pragma once
#include <memory>
#include <d3d11.h>
#include "D3D11Error.h"
#include "D3D11Device.h"
namespace OIV
{
    class D3D11Texture 
    {
    public:

        struct CreateParams
        {
            uint32_t width;
            uint32_t height;
            DXGI_FORMAT format;
        };

        struct InitialBuffer
        {
            uint8_t* buffer;
            uint32_t rowPitchInBytes;
        };
    
        public: // methods
        D3D11Texture(D3D11DeviceSharedPtr device, const CreateParams& createParams, const InitialBuffer* initialBuffer);
        void Use() const;
        ~D3D11Texture();


    private: // methods
        void CreateShaderResourceView();
        void CreateTexture(const InitialBuffer* initialBuffer = nullptr);

    private: // member fields
        D3D11DeviceSharedPtr fDevice;
        ID3D11Texture2D *fTexture = nullptr;
        ID3D11ShaderResourceView* fTextureShaderResourceView = nullptr;
        CreateParams fCreateparams;
    };


    using D3D11TextureSharedPtr = std::shared_ptr<D3D11Texture>;
}