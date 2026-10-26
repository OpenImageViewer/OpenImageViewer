#include "D3D11/D3D11Texture.h"
#include "Image.h"
#include "OIVD3DHelper.h"
#include <ImageUtil/ImageUtil.h>

namespace OIV
{
    D3D11TextureSharedPtr OIVD3DHelper::CreateTexture(D3D11DeviceSharedPtr device, IMCodec::ImageSharedPtr image,
                                                      bool createMipMaps)
    {
        // Viewer images arrive as RGBA, but direct OIV clients can supply native CMYK.
        // Keep conversion ownership local until CreateTexture2D has consumed the pixels.
        DXGI_FORMAT textureFormat = DXGI_FORMAT_UNKNOWN;

        switch (image->GetTexelFormat())
        {
            case IMCodec::TexelFormat::I_B8_G8_R8_A8:
                textureFormat = DXGI_FORMAT_B8G8R8A8_UNORM;
                break;
            case IMCodec::TexelFormat::I_R8_G8_B8_A8:
                textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            case IMCodec::TexelFormat::I_C8_M8_Y8_K8:
                image = IMUtil::ImageUtil::Convert(image, IMCodec::TexelFormat::I_R8_G8_B8_A8);
                if (image == nullptr)
                    D3D11Error::HandleError("Could not convert CMYK image for D3D11 rendering");
                textureFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                break;
            default:
                D3D11Error::HandleError("Unsupported texture format");
        }

        const D3D11Texture::CreateParams params{
            .width  = image->GetWidth(),
            .height = image->GetHeight(),
            .format = textureFormat,
            .mips   = createMipMaps ? -1 : -2,
        };
        const D3D11Texture::InitialBuffer buffer{
            .buffer          = image->GetBuffer(),
            .rowPitchInBytes = image->GetRowPitchInBytes(),
        };

        return std::make_shared<D3D11Texture>(device, params, &buffer);
    }
}  // namespace OIV