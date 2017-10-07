#pragma once
#include "D3D11/D3D11Texture.h"
#include "Image.h"

namespace OIV
{
    class OIVD3DHelper
    {
    public:
        static D3D11TextureSharedPtr CreateTexture(D3D11DeviceSharedPtr device, const IMCodec::ImageSharedPtr image);
    };


}