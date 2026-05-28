#pragma once

#include <LLUtils/StringDefs.h>
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
        void SetSourceFileName(const LLUtils::native_string_type& fileName);
        void SetSourceCode(const std::string& sourceCode);
        void Load();
        const std::string& Getsource() const;
        const LLUtils::native_string_type& GetsourceFileName() const;
        const LLUtils::Buffer& GetShaderData() const;
        void Use();
        virtual ~D3D11Shader() {};

      protected:  // types

        struct ShaderCompileParams
        {
            std::string target;
        };

      protected:  // virtual methods

        virtual void GetShaderCompileParams(ShaderCompileParams& compileParams) = 0;
        virtual void CreateImpl()                                               = 0;
        virtual void UseImpl()                                                  = 0;

      protected:  // methods

        D3D11DeviceSharedPtr GetDevice() const;

      private:  // methods

        void Create();
        UINT GetCompileFlags() const;
        void Compile();
        void HandleCompileError(ID3DBlob* errors) const;

      private:  // member fields

        D3D11DeviceSharedPtr fDevice;
        std::string fSourceCode;
        LLUtils::native_string_type fSourceFileName;
        LLUtils::Buffer fShaderData;
    };

    typedef std::shared_ptr<D3D11Shader> D3D11ShaderUniquePtr;

}  // namespace OIV
