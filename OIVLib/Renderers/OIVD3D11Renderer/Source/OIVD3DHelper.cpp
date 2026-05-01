#include "D3D11/D3D11Texture.h"
#include "Image.h"
#include "OIVD3DHelper.h"

namespace OIV
{
        D3D11TextureSharedPtr OIVD3DHelper::CreateTexture(D3D11DeviceSharedPtr device, const IMCodec::ImageSharedPtr image, bool createMipMaps)
        {

            DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;

            switch (image->GetTexelFormat())
            {
            case IMCodec::TexelFormat::I_B8_G8_R8_A8:
                textureFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
                break;
            case IMCodec::TexelFormat::I_R8_G8_B8_A8:
                textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            default:
                D3D11Error::HandleError("Unsupported texture format");
            }

            
                


            D3D11Texture::CreateParams params;
            params.format = textureFormat;
            params.width = image->GetWidth();
            params.height = image->GetHeight();
            params.mips = createMipMaps ? -1 : -2;


            D3D11Texture::InitialBuffer buffer;
            buffer.buffer = image->GetBuffer();
            buffer.rowPitchInBytes = image->GetRowPitchInBytes();
            

            return std::make_shared<D3D11Texture>(device, params, &buffer);

        }
}