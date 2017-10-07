#pragma once
#include "D3D11/D3D11Texture.h"
#include "Image.h"
#include "OIVD3DHelper.h"

namespace OIV
{
        D3D11TextureSharedPtr OIVD3DHelper::CreateTexture(D3D11DeviceSharedPtr device, const IMCodec::ImageSharedPtr image)
        {

            if (image->GetImageType() != IMCodec::TF_I_R8_G8_B8_A8)
                D3D11Error::HandleError("Direct3D11 renderer supports only RGBA pixel format");


            D3D11Texture::CreateParams params;
            params.format = DXGI_FORMAT_R8G8B8A8_UNORM;
            params.width = image->GetWidth();
            params.height = image->GetHeight();

            D3D11Texture::InitialBuffer buffer;
            buffer.buffer = image->GetBuffer();
            buffer.rowPitchInBytes = image->GetRowPitchInBytes();
            

            return D3D11TextureSharedPtr(new D3D11Texture(device, params, &buffer));

        }
}