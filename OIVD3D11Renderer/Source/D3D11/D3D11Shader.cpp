#include "d3d11shader.h"
#include "D3D11Blob.h"

namespace  OIV
{
    D3D11Shader::D3D11Shader(D3D11DeviceSharedPtr d3dDevice)
        : fDevice(d3dDevice)
    {

    }

    void D3D11Shader::Load(std::string sourceCode)
    {
        fSourceCode = sourceCode;
        Compile();
        Create();
    }

    void D3D11Shader::Load(BlobSharedPtr blob)
    {
        fShaderData = blob;
        Create();
    }

    const std::string& D3D11Shader::Getsource() const
    {
        return fSourceCode;
    }

    const BlobSharedPtr D3D11Shader::GetShaderData() const
    {
        return fShaderData;
    }

    void D3D11Shader::Use()
    {
        UseImpl();
    }

    D3D11DeviceSharedPtr D3D11Shader::GetDevice() const
    {
        return fDevice;
    }

    void D3D11Shader::Create()
    {
        CreateImpl();
    }

    void D3D11Shader::Compile()
    {
        D3D_SHADER_MACRO macros[2];
        macros[0].Definition = "1";
        macros[0].Name = "HLSL";

        macros[1].Definition = nullptr;
        macros[1].Name = nullptr;

        ComPtr<ID3DBlob> microCode;
        ComPtr<ID3DBlob> errors;
        HRESULT res;

        ShaderCompileParams params;

        GetShaderCompileParams(params);

        res =
            D3DCompile(
                static_cast<const void*>(fSourceCode.c_str())
                , fSourceCode.length()
                , nullptr /*source name*/
                , &macros[0]
                , nullptr
                , "main"
                , params.target.c_str()
                , D3DCOMPILE_OPTIMIZATION_LEVEL3
                , 0
                , microCode.GetAddressOf()
                , errors.GetAddressOf()
            );

        if (SUCCEEDED(res) == false)
            HandleCompileError(errors.Get());

        fShaderData = BlobSharedPtr(new Blob(microCode.Get()));
    }

    void D3D11Shader::HandleCompileError(ID3DBlob* errors) const
    {
        using namespace std;
        if (errors == nullptr)
            D3D11Error::HandleError("Direct3D11 raised a logic error.\nReason: misuse of error handling - no error");

        std::size_t size = errors->GetBufferSize();
        unique_ptr<char> errorString = unique_ptr<char>(new char[size]);
        memcpy(errorString.get(), errors->GetBufferPointer(), size);

        string errorMessage = string("Direct3D11 Can not compile GPU program.\nreason: ") + errorString.get();

        D3D11Error::HandleError(errorMessage);
    }
}
