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

        IUnknown* CreateImpl()  override
        {
            ComPtr<ID3D11VertexShader> vertexShader;
            
            D3D11Error::HandleDeviceError(GetDevice()->GetdDevice()->CreateVertexShader(GetShaderData()->buffer, GetShaderData()->size, nullptr, vertexShader.ReleaseAndGetAddressOf())
                , " could not create vertex shader from microcode");
         
            IUnknown* result = nullptr;
#ifdef _DEBUG
            std::string obj = "Vertex shader";
            vertexShader->SetPrivateData(WKPDID_D3DDebugObjectName, obj.size(), obj.c_str());
#endif

            D3D11Error::HandleDeviceError(vertexShader.Get()->QueryInterface(&result));
            
            return result;
        }

        void UseImpl() override
        {
            ID3D11VertexShader* vertexShader = nullptr;
            D3D11Error::HandleDeviceError(GetShader()->QueryInterface(&vertexShader));
            GetDevice()->GetContext()->VSSetShader(vertexShader, nullptr, 0);
            vertexShader->Release();
        }
    };
}
