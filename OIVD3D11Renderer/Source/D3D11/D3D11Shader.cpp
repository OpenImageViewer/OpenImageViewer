#include "d3d11shader.h"
#include <FileHelper.h>
#include "D3D11Utility.h"

namespace  OIV
{

    class D3D11IncludeHandler final : public ID3DInclude
    {
    public:

        D3D11IncludeHandler(D3D11Shader* shader)
        {
            fShader = shader;
        }

        HRESULT __stdcall Open(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName,
            LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes) override
        {
            using namespace std::experimental;
            filesystem::path p = fShader->GetsourceFileName();
            p = p.parent_path() / pFileName;
            
            LLUtils::Buffer buf = LLUtils::File::ReadAllBytes(p);

            std::byte* buffer;
            size_t bufferSize;
            buf.RemoveOwnership(bufferSize, buffer);
            *pBytes = static_cast<UINT>(bufferSize);
            *ppData = buffer;
            return S_OK;
            
        }

        HRESULT __stdcall Close(LPCVOID pData) override
        {
            LLUtils::Buffer::GlobalDealocate(reinterpret_cast<std::byte*>( const_cast<void*>(pData)));
            return S_OK;
        }

    private:
        D3D11Shader* fShader;
        
    };


    D3D11Shader::D3D11Shader(D3D11DeviceSharedPtr d3dDevice)
        : fDevice(d3dDevice)
    {

    }

    void D3D11Shader::SetMicroCode(LLUtils::Buffer&& blob)
    {
        fShaderData = std::move(blob);
    }


    void D3D11Shader::SetSourceFileName(const std::wstring& fileName)
    {
        fSourceFileName = fileName;
    }

    void D3D11Shader::SetSourceCode(const std::string& sourceCode)
    {
        fSourceCode = sourceCode;
    }

    void D3D11Shader::Load()
    {
        if (fShaderData == nullptr)
        {
            if (fSourceCode.empty() == true)
            {
                if (fSourceFileName.empty() == true)
                    D3D11Error::HandleError("Direct3D11 could not locate the GPU programs");

                fSourceCode = LLUtils::File::ReadAllText(GetsourceFileName());
            }
            
            if (fSourceCode.empty() == true)
                D3D11Error::HandleError("Direct3D11 could not locate the GPU programs");

            Compile();
        }

        if (fShaderData == nullptr)
            D3D11Error::HandleError("Could not compile GPU program");

        Create();
    }

    const std::string& D3D11Shader::Getsource() const
    {
        return fSourceCode;
    }

    const std::wstring& D3D11Shader::GetsourceFileName() const
    {
        return fSourceFileName;
    }

    const LLUtils::Buffer& D3D11Shader::GetShaderData() const
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

    UINT D3D11Shader::GetCompileFlags() const
    {
        return 
              D3DCOMPILE_OPTIMIZATION_LEVEL3 
            | D3DCOMPILE_IEEE_STRICTNESS 
            | D3DCOMPILE_ENABLE_STRICTNESS
            | D3DCOMPILE_WARNINGS_ARE_ERRORS
        ;
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

        D3D11IncludeHandler includeHandler(this);
        GetShaderCompileParams(params);
        
        res =
            D3DCompile(
                static_cast<const void*>(fSourceCode.c_str())
                , fSourceCode.length()
                , fSourceFileName.empty() == false ? LLUtils::StringUtility::ToAString(fSourceFileName).c_str() : nullptr /*source name*/
                , &macros[0]
                , &includeHandler
                , "main"
                , params.target.c_str()
                , GetCompileFlags() 
                , 0
                , microCode.GetAddressOf()
                , errors.GetAddressOf()
            );

        if (SUCCEEDED(res) == false)
            HandleCompileError(errors.Get());

        fShaderData = D3D11Utility::BufferFromBlob(microCode.Get());
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
