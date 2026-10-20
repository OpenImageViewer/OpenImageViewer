#include <RendererFactory.h>
#include "RendererSelection.h"
#include "NullRenderer.h"
#include <stdexcept>

namespace OIV
{
    RendererCreation CreateRenderer(NativeRenderTarget target, const std::filesystem::path& cacheDirectory,
                                    const RendererOptions& options)
    {
        if (const auto error = ValidateRendererOptions(options); !error.empty())
            throw std::invalid_argument(error);
        const RendererInitialization initialization{
            .container      = target.container,
            .nativeDisplay  = target.nativeDisplay,
            .cacheDirectory = cacheDirectory,
        };
        BackendSelection selected;
#if OIV_ALLOW_NULL_RENDERER
        if (options.renderer == RendererType::Null)
        {
            selected.backend = std::make_unique<NullRenderer>();
            selected.backend->Init(initialization);
        }
        else
#endif
            selected = SelectRenderer(GetRendererBackends(), options, initialization);
        return {.renderer = std::make_unique<Renderer>(std::move(selected.backend)), .type = selected.type};
    }
}  // namespace OIV
