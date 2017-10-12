#pragma once
#include "D3D11Shader.h"
#include "D3D11Blob.h"

namespace OIV
{
  
    class D3D11VertexShader : public D3D11Shader
    {
    public:
        D3D11VertexShader(D3D11DeviceSharedPtr d3dDevice) :
            D3D11Shader(d3dDevice)
        {
            
        }

    protected:

        void GetShaderCompileParams(ShaderCompileParams& compileParams) override
        {
            compileParams.target = "vs_4_0";
        }
        
        void CreateImpl()  override
        {
            D3D11Error::HandleDeviceError(GetDevice()->GetdDevice()->CreateVertexShader(GetShaderData()->buffer, GetShaderData()->size, nullptr, fVertexShader.ReleaseAndGetAddressOf())
                , " could not create vertex shader from microcode");

            OIV_D3D_SET_OBJECT_NAME(fVertexShader, "Vertex shader");
        }

        void UseImpl() override
        {
            GetDevice()->GetContext()->VSSetShader(fVertexShader.Get(), nullptr, 0);
        }
    private:
        ComPtr<ID3D11VertexShader> fVertexShader;
    };
}
