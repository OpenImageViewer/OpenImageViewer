#include "RendererSelection.h"
#if OIV_BUILD_RENDERER_VK
    #include <OIVVKRendererFactory.h>
#endif
#if OIV_BUILD_RENDERER_D3D11
    #include <OIVD3D11RendererFactory.h>
#endif
#if OIV_BUILD_RENDERER_GL
    #include <OIVGLRendererFactory.h>
#endif

namespace OIV
{
    namespace
    {
        // A single API order serves both targets; CMake rejects D3D11 outside Windows.
        constexpr std::array<RendererBackend, OIV_BUILD_RENDERER_VK + OIV_BUILD_RENDERER_D3D11 + OIV_BUILD_RENDERER_GL>
            Backends{{
#if OIV_BUILD_RENDERER_VK
                {{RendererType::Vulkan, "Vulkan", true}, &VKRendererFactory::Create},
#endif
#if OIV_BUILD_RENDERER_D3D11
                {{RendererType::D3D11, "D3D11", true}, &D3D11RendererFactory::Create},
#endif
#if OIV_BUILD_RENDERER_GL
                {{RendererType::OpenGL, "GL", false}, &GLRendererFactory::Create},
#endif
            }};
        static_assert(Backends.size() <= MaxRendererCount);
        constexpr auto Metadata = []
        {
            std::array<RendererInfo, Backends.size()> result{};
            for (size_t i = 0; i < Backends.size(); ++i)
                result[i] = Backends[i].info;
            return result;
        }();
        static_assert(
            []
            {
                for (size_t i = 0; i < Backends.size(); ++i)
                {
                    if (Backends[i].info.type == RendererType::Null || Backends[i].info.name.empty() ||
                        !Backends[i].create)
                        return false;
                    for (size_t j = 0; j < i; ++j)
                        if (Backends[i].info.type == Backends[j].info.type ||
                            detail::AsciiEqual(Backends[i].info.name, Backends[j].info.name))
                            return false;
                }
                return true;
            }());
    }  // namespace

    std::span<const RendererInfo> GetBuiltRenderers()
    {
        return Metadata;
    }
    std::span<const RendererBackend> GetRendererBackends()
    {
        return Backends;
    }
    RendererType GetDefaultRenderer()
    {
        return Metadata.empty() ? RendererType::Null : Metadata.front().type;
    }
    bool IsRendererAvailable(RendererType renderer)
    {
        return (OIV_ALLOW_NULL_RENDERER && renderer == RendererType::Null) ||
               std::ranges::any_of(Metadata, [renderer](const auto& info) { return info.type == renderer; });
    }
    std::string ValidateRendererOptions(const RendererOptions& options)
    {
        std::string error;
        if (options.renderer && !IsRendererAvailable(*options.renderer))
            error = "Requested renderer is not available in this build";
        else if (!options.renderer && Metadata.empty())
            error = "No viewer renderer is built; Null is available only through explicit internal selection";
        else if (options.adapterIndex && *options.adapterIndex < 0)
            error = "--adapter_index must be nonnegative";
        else if (!options.adapterIndex && options.adapterName && detail::TrimAdapterName(*options.adapterName).empty())
            error = "--adapter_name requires a nonempty vendor or GPU-name substring";
        else if (options.adapterIndex || options.adapterName)
        {
            const auto api = options.renderer       ? options.renderer
                             : options.adapterIndex ? std::optional{GetDefaultRenderer()}
                                                    : std::nullopt;
            if (!std::ranges::any_of(Metadata, [&](const auto& info)
                                     { return (!api || info.type == *api) && info.supportsAdapterSelection; }))
                error = api ? "The selected API does not support explicit adapter selection; GL uses the "
                              "platform-selected adapter"
                            : "No built renderer supports explicit adapter selection; GL uses the platform-selected "
                              "adapter";
        }
        return error;
    }
}  // namespace OIV
