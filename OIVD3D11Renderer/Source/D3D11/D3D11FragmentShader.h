#pragma once
#include "D3D11Shader.h"

namespace OIV
{

    class D3D11FragmentShader : public D3D11Shader
    {
    public:
        D3D11FragmentShader(D3D11DeviceSharedPtr d3dDevice) :
            D3D11Shader(d3dDevice)
        {

        }
    protected:
        void GetShaderCompileParams(ShaderCompileParams& compileParams) override
        {
            compileParams.target = "ps_4_0";
        }

        IUnknown* CreateImpl()  override
        {
            ComPtr<ID3D11PixelShader> pixelShader;
            D3D11Error::HandleDeviceError(GetDevice()->GetdDevice()->CreatePixelShader(GetShaderData()->buffer, GetShaderData()->size, nullptr, &pixelShader)
                , " could not create fragment shader from microcode");

#ifdef _DEBUG
            std::string obj = "Fragment shader";
            pixelShader->SetPrivateData(WKPDID_D3DDebugObjectName, obj.size(), obj.c_str());
#endif
            IUnknown* result = nullptr;

            D3D11Error::HandleDeviceError(pixelShader.Get()->QueryInterface(&result));
            
            return result;
        }

        void UseImpl() override
        {
            ID3D11PixelShader* shader = nullptr;
            D3D11Error::HandleDeviceError(GetShader()->QueryInterface(&shader));
            GetDevice()->GetContext()->PSSetShader(shader, nullptr, 0);
            shader->Release();
        }
    };
}
