#pragma once
#include <Interfaces/IRenderer.h>

namespace OIV
{
    inline constexpr std::size_t MaxRendererCount = 3;
    struct RendererBackend
    {
        RendererInfo info;
        IRendererSharedPtr (*create)();
    };

    // Startup-only selection; recovery keeps the chosen API. Exhaust Hardware, Unknown, then Software,
    // using compiled API order within each tier. Options constrain candidates; discover APIs lazily.
    // Destroy failed/deferred instances before the next factory call. Stop at success; exhaustion
    // throws collected diagnostics. dataPath is a base directory; append the backend name for each
    // synchronous initialization call.
    IRendererSharedPtr SelectRenderer(std::span<const RendererBackend> backends, const RendererOptions& options,
                                      const OIV_RendererInitializationParams& params);
    std::span<const RendererBackend> GetRendererBackends();
}  // namespace OIV
