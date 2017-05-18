#pragma once
#include "D3D11Common.h"
#include <d3dcompiler.h>
#include <memory>
#include "D3D11Error.h"

namespace OIV
{

    class D3D11Shader
    {
    public:
        D3D11Shader(D3D11DeviceSharedPtr d3dDevice);
        virtual ~D3D11Shader();
        void Load(std::string sourceCode);
        void Load(BlobSharedPtr blob);
        IUnknown* GetShader();
        const std::string& Getsource() const;
        const BlobSharedPtr GetShaderData() const;
        void Use();

    protected: // types
        struct ShaderCompileParams
        {
            std::string target;
        };
    protected: // virtual methods
        virtual void GetShaderCompileParams(ShaderCompileParams& compileParams) = 0;
        virtual IUnknown* CreateImpl() = 0;
        virtual void UseImpl() = 0;

    protected: // methods
        D3D11DeviceSharedPtr GetDevice() const;

    private: // methods
        void Create();
        void Compile();
        void HandleCompileError(ID3DBlob* errors) const;

    private: // member fields
        D3D11DeviceSharedPtr fDevice;
        std::string fSourceCode;
        BlobSharedPtr fShaderData;
        IUnknown* fShader;
    };

    typedef std::shared_ptr<D3D11Shader> D3D11ShaderUniquePtr;

}
