#pragma once
#include "D3D11Shader.h"
#include <FileHelper.h>
#include "D3D11Blob.h"

namespace OIV
{
    class D3D11Utility
    {
    public:

        static void D3D11Utility::CreateD3D11DefaultBlendState(D3D11_BLEND_DESC& blend)
        {
            memset(&blend, 0, sizeof(blend));
            D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend.RenderTarget[0];
            blend.AlphaToCoverageEnable = FALSE;
            blend.IndependentBlendEnable = FALSE;
            rt0.BlendEnable = FALSE;
            rt0.SrcBlend = D3D11_BLEND_ONE;
            rt0.DestBlend = D3D11_BLEND_ZERO;
            rt0.BlendOp = D3D11_BLEND_OP_ADD;
            rt0.SrcBlendAlpha = D3D11_BLEND_ONE;
            rt0.DestBlendAlpha = D3D11_BLEND_ZERO;
            rt0.BlendOpAlpha = D3D11_BLEND_OP_ADD;
            rt0.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        }

        static void D3D11Utility::CreateD3D11DefaultSamplerState(D3D11_SAMPLER_DESC &sampler)
        {
            sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.MinLOD = -FLT_MAX;
            sampler.MaxLOD = FLT_MAX;
            sampler.MipLODBias = 0.0f;
            sampler.MaxAnisotropy = 1;
            sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
            sampler.BorderColor[0] = sampler.BorderColor[1] = sampler.BorderColor[2] = sampler.BorderColor[3] = 1.0f;
        }

        static void D3D11Utility::LoadShader(D3D11ShaderUniquePtr& shader, const std::experimental::filesystem::path& shaderPath)
        {
            using namespace std::experimental;

            filesystem::path oivAppDataFolder = LLUtils::PlatformUtility::GetAppDataFolder();

            //Try cache first;
            filesystem::path cachePath = (oivAppDataFolder / L"ShaderCache" / shaderPath.filename()).replace_extension(L"bin");
#ifndef DEBUG
            if (filesystem::exists(cachePath))
            {
                //Load from cache 
                BlobSharedPtr blob = BlobSharedPtr(new Blob());
                LLUtils::File::ReadAllBytes(cachePath, blob->size, blob->buffer);
                shader->Load(blob);
            }
            else 
#endif

            if (filesystem::exists(shaderPath))
            {
                std::string shaderSource = LLUtils::File::ReadAllText(shaderPath);
                // compile source and save cache
                if (shaderSource.empty() == true || shaderSource.empty() == true)
                    D3D11Error::HandleError("Direct3D11 could not locate the GPU programs");

                shader->Load(shaderSource);
                BlobSharedPtr blob = shader->GetShaderData();
                LLUtils::File::WriteAllBytes(cachePath, blob->size, blob->buffer);
            }
        }
    };
}
