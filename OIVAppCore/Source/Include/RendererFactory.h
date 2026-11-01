#pragma once
#include <RendererCore/Renderer.h>

namespace OIV
{
    // Identity belongs to the successful candidate, which may differ from the requested API.
    struct RendererCreation
    {
        std::unique_ptr<Renderer> renderer;
        RendererType type = RendererType::Null;
    };

    // Select and initialize privately, publishing an owner only after startup succeeds.
    // Release every RenderImage before destroying this owner or replacing it.
    // An empty cache directory disables persistent caches. Resource lookup remains installation-relative.
    RendererCreation CreateRenderer(NativeRenderTarget target, const std::filesystem::path& cacheDirectory,
                                    const RendererOptions& options = {});
}  // namespace OIV
