#pragma once
#include <RendererCore/RendererOptions.h>
#include <span>

namespace OIV
{
    // Compiled APIs in startup preference order, without probing the graphics runtime.
    std::span<const RendererInfo> GetBuiltRenderers();
    RendererType GetDefaultRenderer();
    bool IsRendererAvailable(RendererType renderer);
    std::string ValidateRendererOptions(const RendererOptions& options);
}  // namespace OIV
