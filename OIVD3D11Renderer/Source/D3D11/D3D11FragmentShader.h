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

        void CreateImpl()  override
        {
            D3D11Error::HandleDeviceError(GetDevice()->GetdDevice()->CreatePixelShader(GetShaderData().GetBuffer(), GetShaderData().Size(), nullptr, fPixelShader.ReleaseAndGetAddressOf())
                , " could not create fragment shader from microcode");

            OIV_D3D_SET_OBJECT_NAME(fPixelShader, "Fragment shader");
        }

        void UseImpl() override
        {
            GetDevice()->GetContext()->PSSetShader(fPixelShader.Get(), nullptr, 0);
        }

    private:
        ComPtr<ID3D11PixelShader> fPixelShader;
    };
}
