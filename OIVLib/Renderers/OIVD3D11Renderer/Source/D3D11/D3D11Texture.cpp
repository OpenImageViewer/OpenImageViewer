#include "D3D11Texture.h"

namespace OIV
{
    D3D11Texture::D3D11Texture(D3D11DeviceSharedPtr device, const CreateParams& createParams, const InitialBuffer* initialBuffer) : fDevice(device) , fCreateparams(createParams)
    {
        CreateTexture(initialBuffer);
        CreateShaderResourceView();
    }

    const D3D11Texture::CreateParams& D3D11Texture::GetCreateParams() const
    {
        return fCreateparams;
    }


    void D3D11Texture::CreateShaderResourceView()
    {
        const bool generateMips = fCreateparams.mips == -1;

        UINT numMips = 1;

        if (generateMips)
        {
            D3D11_TEXTURE2D_DESC textureDesc;
            fTexture->GetDesc(&textureDesc);
            numMips = textureDesc.MipLevels;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC viewDesc = {};

        viewDesc.Format = fCreateparams.format;
        viewDesc.Texture2D.MostDetailedMip = 0;
        viewDesc.Texture2D.MipLevels = numMips;
        viewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;

        D3D11Error::HandleDeviceError(
            fDevice->GetdDevice()->CreateShaderResourceView(fTexture.Get(), &viewDesc, fTextureShaderResourceView.ReleaseAndGetAddressOf())
            , "Can not create 'Shader resource view'");

        if (generateMips)
            fDevice->GetContext()->GenerateMips(fTextureShaderResourceView.Get());
    }

    void D3D11Texture::CreateTexture(const InitialBuffer* initialBuffer)
    {
        const bool generateMips = fCreateparams.mips == -1;

        D3D11_TEXTURE2D_DESC desc {};
        desc.Width = static_cast<UINT>(fCreateparams.width);
        desc.Height = static_cast<UINT>(fCreateparams.height);
        desc.MipLevels = generateMips ? 0 : 1;
        desc.ArraySize = 1;
        desc.Format = fCreateparams.format;
        desc.SampleDesc.Count = 1;
        desc.Usage = generateMips ? D3D11_USAGE_DEFAULT : D3D11_USAGE_IMMUTABLE ;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | (generateMips ? D3D11_BIND_RENDER_TARGET : 0);
        desc.CPUAccessFlags = static_cast<UINT>(0);

        
        desc.MiscFlags = generateMips ? D3D11_RESOURCE_MISC_GENERATE_MIPS : 0;

        D3D11_SUBRESOURCE_DATA subResourceData = {};
        D3D11_SUBRESOURCE_DATA* subResourceDataForAPI = nullptr;

        if (initialBuffer != nullptr)
        {
            subResourceData.pSysMem = initialBuffer->buffer;
            subResourceData.SysMemPitch = initialBuffer->rowPitchInBytes;
            subResourceDataForAPI = &subResourceData;


            D3D11Error::HandleDeviceError(fDevice->GetdDevice()->CreateTexture2D(&desc
                , generateMips ? nullptr : subResourceDataForAPI
                , fTexture.ReleaseAndGetAddressOf())
                , "Can not create texture");


            if (generateMips)
            {
                D3D11_BOX box;
                box.back = 1;
                box.front = 0;
                box.left = 0;
                box.top = 0;
                box.right = desc.Width;
                box.bottom = desc.Height;
                fDevice->GetContext()->UpdateSubresource(fTexture.Get(), 0, &box, (void*)initialBuffer->buffer, initialBuffer->rowPitchInBytes, initialBuffer->rowPitchInBytes * desc.Height);
            }
        }
        else
        {
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "Texture buffer cannot be null");
        }

        OIV_D3D_SET_OBJECT_NAME(fTexture, "Texture2D");
    }

    void D3D11Texture::Use() const
    {
        fDevice->GetContext()->PSSetShaderResources(static_cast<UINT>(0), static_cast<UINT>(1), fTextureShaderResourceView.GetAddressOf());
    }
}
