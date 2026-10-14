#pragma once
#include <RendererCore/RenderTypes.h>
#include <Image.h>
#include <RendererCore/ViewParameters.h>
#include <map>
#include <cstdint>

namespace OIV
{
    // Desired presentation state is independent of pixel content and upload revisions.
    struct RenderImageProperties
    {
        // Placement in client pixels, following the viewer's existing rounding policy.
        LLUtils::PointF64 position = LLUtils::PointF64::Zero;
        // Client pixels per image texel, one for software-resampled output.
        LLUtils::PointF64 scale = LLUtils::PointF64::One;
        // Multiplicative opacity, zero suppresses upload and drawing.
        double opacity = 1.0;
        // Whether this presentation participates in the current frame.
        bool visible = true;
        // Per-image sampling policy, preserving explicit call-site defaults.
        OIV::FilterType filterType = OIV::FilterType::Lanczos3;
        // Main images precede selection, overlays follow it in registration order.
        OIV::ImageRenderMode imageRenderMode = OIV::ImageRenderMode::MainImage;
    };

    // Renderer-owned CPU bookkeeping. Backends borrow records only during Render.
    struct RenderImageRecord
    {
        // Published replacement-only CPU pixels, empty for an empty registration.
        IMCodec::ImageSharedPtr pixels;
        // Effective placement and appearance for this registration.
        RenderImageProperties properties;
        // Content version, changed only when the shared pixel object changes.
        uint64_t revision = 1;
    };

    // Internal frame view borrows map storage without allocating a snapshot.
    struct RenderFrame
    {
        // Ordered renderer-local IDs and records, valid only for the Render call.
        const std::map<uint64_t, RenderImageRecord>& images;
    };
}  // namespace OIV
