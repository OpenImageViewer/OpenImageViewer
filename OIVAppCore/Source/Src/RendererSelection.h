#pragma once
#include <RendererCore/IRenderBackend.h>
#include <RendererDiscovery.h>

namespace OIV
{
    inline constexpr std::size_t MaxRendererCount = 3;
    // Startup discovery entry whose factory returns only the graphics contract.
    struct RendererBackend
    {
        // Compiled backend identity and capability information used by selection policy.
        RendererInfo info;
        // Factory invoked lazily while evaluating the existing candidate order.
        std::unique_ptr<OIV::IRenderBackend> (*create)();
    };

    struct BackendSelection
    {
        std::unique_ptr<IRenderBackend> backend;
        RendererType type = RendererType::Null;
    };

    // Startup-only selection; recovery keeps the chosen API. Exhaust Hardware, Unknown, then Software,
    // using compiled API order within each tier. Options constrain candidates; discover APIs lazily.
    // Destroy failed/deferred instances before the next factory call. Stop at success; exhaustion
    // throws collected diagnostics. cacheDirectory is a base directory; append the backend name for each
    // synchronous initialization call.
    BackendSelection SelectRenderer(std::span<const RendererBackend> backends, const RendererOptions& options,
                                    const OIV::RendererInitialization& params);
    std::span<const RendererBackend> GetRendererBackends();
}  // namespace OIV
