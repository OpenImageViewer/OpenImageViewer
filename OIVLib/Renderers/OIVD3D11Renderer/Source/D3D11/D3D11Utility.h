#pragma once
#include <cfloat>
#include <d3d11.h>
#include <RendererCore/RenderTypes.h>
#include <LLUtils/FileHelper.h>
#include "D3D11Shader.h"
#include <OIVShared/AtomicFile.h>
#include <LLUtils/Logging/Logger.h>

namespace OIV
{
    class D3D11Utility
    {
      public:

        static LLUtils::Buffer BufferFromBlob(ID3D10Blob* blob)
        {
            LLUtils::Buffer buffer(blob->GetBufferSize());
            buffer.Write(static_cast<const std::byte*>(blob->GetBufferPointer()), 0, buffer.Size());
            return buffer;
        }

        static void CreateD3D11DefaultBlendState(D3D11_BLEND_DESC& blend)
        {
            memset(&blend, 0, sizeof(blend));
            D3D11_RENDER_TARGET_BLEND_DESC& rt0 = blend.RenderTarget[0];
            blend.AlphaToCoverageEnable         = FALSE;
            blend.IndependentBlendEnable        = FALSE;
            rt0.BlendEnable                     = FALSE;
            rt0.SrcBlend                        = D3D11_BLEND_ONE;
            rt0.DestBlend                       = D3D11_BLEND_ZERO;
            rt0.BlendOp                         = D3D11_BLEND_OP_ADD;
            rt0.SrcBlendAlpha                   = D3D11_BLEND_ONE;
            rt0.DestBlendAlpha                  = D3D11_BLEND_ZERO;
            rt0.BlendOpAlpha                    = D3D11_BLEND_OP_ADD;
            rt0.RenderTargetWriteMask           = D3D11_COLOR_WRITE_ENABLE_ALL;
        }

        static void CreateD3D11DefaultSamplerState(D3D11_SAMPLER_DESC& sampler)
        {
            sampler.Filter         = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
            sampler.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
            sampler.MinLOD         = -FLT_MAX;
            sampler.MaxLOD         = FLT_MAX;
            sampler.MipLODBias     = 0.0f;
            sampler.MaxAnisotropy  = 1;
            sampler.ComparisonFunc = D3D11_COMPARISON_NEVER;
            sampler.BorderColor[0] = sampler.BorderColor[1] = sampler.BorderColor[2] = sampler.BorderColor[3] = 1.0f;
        }

        static void LoadShader(D3D11ShaderUniquePtr& shader, const std::filesystem::path& cacheFolder)
        {
            const auto cachePath = cacheFolder.empty()
                                       ? std::filesystem::path{}
                                       : (cacheFolder / std::filesystem::path(shader->GetsourceFileName()).filename())
                                             .replace_extension("bin");
            // Development builds must pick up shader edits even when the application version is unchanged.
#ifdef _DEBUG
            constexpr bool readCache = false;
#else
            constexpr bool readCache = true;
#endif
            bool loaded = false;
            if (readCache && !cachePath.empty())
            {
                try
                {
                    if (std::filesystem::exists(cachePath))
                    {
                        auto code = LLUtils::File::ReadAllBytes(cachePath);
                        if (code.Size() != 0)
                        {
                            shader->SetMicroCode(std::move(code));
                            shader->Load();
                            loaded = true;
                        }
                    }
                }
                catch (const std::exception&)
                {
                    // Cached bytecode is disposable. Release it so Load recompiles from the packaged source.
                    shader->SetMicroCode({});
                }
            }
            if (!loaded)
            {
                shader->Load();
                if (!cachePath.empty())
                {
                    try
                    {
                        const auto& code = shader->GetShaderData();
                        WriteFileAtomically(cachePath, {code.data(), code.Size()});
                    }
                    catch (const std::exception&)
                    {
                        static const auto category = LLUtils::Logger::RegisterCategory("ShaderCache");
                        LL_LOG(category, LLUtils::LogLevel::Warning, "Could not save the compiled shader cache");
                    }
                }
            }
        }
    };
}  // namespace OIV
