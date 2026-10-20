#pragma once

#include <RendererCore/RendererOptions.h>
#include <LLUtils/Color.h>
#include <cstddef>
#include <filesystem>
#include <optional>

namespace OIV
{
    enum class FilterType
    {
        None,
        Linear,
        Lanczos3,
        Count
    };
    enum class ImageRenderMode
    {
        Default,
        MainImage,
        Overlay
    };
    enum class TransparencyShade
    {
        Light,
        Medium,
        Dark,
        Darker,
        Count
    };

    struct ColorExposure
    {
        double exposure                             = 1.0;
        double offset                               = 0.0;
        double gamma                                = 1.0;
        double saturation                           = 1.0;
        double contrast                             = 1.0;
        bool operator==(const ColorExposure&) const = default;
    };

    // Native handles are borrowed until rendering stops, before window teardown.
    struct NativeRenderTarget
    {
        std::size_t container = 0;
        void* nativeDisplay   = nullptr;
    };

    // Cache paths are owned values. Backend initialization borrows native handles.
    struct RendererInitialization
    {
        std::size_t container = 0;
        void* nativeDisplay   = nullptr;
        std::filesystem::path cacheDirectory;
        int gpuIndex            = -1;
        const char* adapterName = nullptr;
        std::optional<Acceleration> acceleration;
    };
}  // namespace OIV
