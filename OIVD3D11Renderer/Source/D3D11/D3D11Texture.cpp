#include "D3D11Texture.h"

namespace OIV
{
    D3D11Texture::D3D11Texture(D3D11DeviceSharedPtr device, const CreateParams& createParams, const InitialBuffer* initialBuffer)
    {
        fDevice = device;
        fCreateparams = createParams;
        CreateTexture(initialBuffer);
        CreateShaderResourceView();
    }

    const D3D11Texture::CreateParams& D3D11Texture::GetCreateParams() const
    {
        return fCreateparams;
    }


    void D3D11Texture::CreateShaderResourceView()
    {
        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc;
        memset(&viewDesc, 0, sizeof viewDesc);

        viewDesc.Format = fCreateparams.format;
        viewDesc.Texture2D.MostDetailedMip = 0;
        viewDesc.Texture2D.MipLevels = 1;
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

        D3D11Error::HandleDeviceError(
            fDevice->GetdDevice()->CreateShaderResourceView(fTexture.Get(), &viewDesc, fTextureShaderResourceView.ReleaseAndGetAddressOf())
            , "Can not create 'Shader resource view'");
    }

    void D3D11Texture::CreateTexture(const InitialBuffer* initialBuffer)
    {
        D3D11_TEXTURE2D_DESC desc = {0};
        desc.Width = static_cast<UINT>(fCreateparams.width);
        desc.Height = static_cast<UINT>(fCreateparams.height);
        desc.MipLevels = desc.ArraySize = 1;
        desc.Format = fCreateparams.format;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_IMMUTABLE;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        desc.CPUAccessFlags = static_cast<UINT>(0);
        desc.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA subResourceData = {};
        D3D11_SUBRESOURCE_DATA* subResourceDataForAPI = nullptr;

        if (initialBuffer != nullptr)
        {
            subResourceData.pSysMem = initialBuffer->buffer;
            subResourceData.SysMemPitch = initialBuffer->rowPitchInBytes;
            subResourceDataForAPI = &subResourceData;
        }

        D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateTexture2D(&desc, subResourceDataForAPI, fTexture.ReleaseAndGetAddressOf())
                                      , "Can not create texture");
    }

    void D3D11Texture::Use() const
    {
        fDevice->GetContext()->PSSetShaderResources(static_cast<UINT>(0), static_cast<UINT>(1), fTextureShaderResourceView.GetAddressOf());
    }
}
