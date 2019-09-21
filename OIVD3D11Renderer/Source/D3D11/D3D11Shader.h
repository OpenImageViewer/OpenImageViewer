#pragma once
#include <memory>
#include "D3D11Common.h"
#include <d3dcompiler.h>
#include "D3D11Error.h"
#include <LLUtils/Buffer.h>

namespace OIV
{

    class D3D11Shader
    {
    public:
        D3D11Shader(D3D11DeviceSharedPtr d3dDevice);
        void SetMicroCode(LLUtils::Buffer&& blob);
        void SetSourceFileName(const std::wstring& fileName);
        void SetSourceCode(const std::string& sourceCode);
        void Load();
        const std::string& Getsource() const;
        const std::wstring& GetsourceFileName() const;
        const LLUtils::Buffer& GetShaderData() const;
        void Use();

    protected: // types
        struct ShaderCompileParams
        {
            std::string target;
        };
    protected: // virtual methods
        virtual void GetShaderCompileParams(ShaderCompileParams& compileParams) = 0;
        virtual void CreateImpl() = 0;
        virtual void UseImpl() = 0;

    protected: // methods
        D3D11DeviceSharedPtr GetDevice() const;

    private: // methods
        void Create();
        UINT GetCompileFlags() const;
        void Compile();
        void HandleCompileError(ID3DBlob* errors) const;

    private: // member fields
        D3D11DeviceSharedPtr fDevice;
        std::string fSourceCode;
        std::wstring fSourceFileName;
        LLUtils::Buffer fShaderData;
    };

    typedef std::shared_ptr<D3D11Shader> D3D11ShaderUniquePtr;

}
