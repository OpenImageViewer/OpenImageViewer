#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace OIV
{
    enum class RendererType
    {
        OpenGL,
        D3D11,
        Vulkan,
        Null
    };
    enum class Acceleration
    {
        Hardware,
        Unknown,
        Software
    };

    inline constexpr std::array AccelerationOrder{Acceleration::Hardware, Acceleration::Unknown,
                                                  Acceleration::Software};
    constexpr std::string_view GetAccelerationName(Acceleration acceleration)
    {
        switch (acceleration)
        {
            case Acceleration::Hardware:
                return "Hardware";
            case Acceleration::Software:
                return "Software";
            default:
                return "Unknown";
        }
    }

    struct RendererInfo
    {
        RendererType type;
        std::string_view name;
        bool supportsAdapterSelection;
    };

    // Names are UTF-8. Adapter indices belong to the reporting graphics API.
    struct RendererAdapter
    {
        int index = -1;
        std::string name;
        uint32_t vendorId         = 0;
        Acceleration acceleration = Acceleration::Unknown;
        bool preferred            = false;
        // Selected-device versions are populated during initialization, absent during enumeration.
        std::string apiVersion;
        std::string driverVersion;
    };

    struct RendererOptions
    {
        // Restrict the graphics API. Its eligible adapters and acceleration tiers still participate.
        std::optional<RendererType> renderer;
        // The vendor/name filter persists across API fallback. An adapter index takes precedence.
        std::optional<std::string> adapterName;
        // Zero-based index in the selected or default renderer. Disables adapter and API fallback.
        std::optional<int> adapterIndex;
    };

    namespace detail
    {
        constexpr unsigned char AsciiLower(unsigned char c)
        {
            return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
        }
        constexpr bool AsciiEqual(std::string_view a, std::string_view b)
        {
            return std::ranges::equal(a, b,
                                      [](unsigned char x, unsigned char y) { return AsciiLower(x) == AsciiLower(y); });
        }
        constexpr std::string_view TrimAdapterName(std::string_view name)
        {
            constexpr std::string_view whitespace = " \t\r\n\f\v";
            const auto first                      = name.find_first_not_of(whitespace);
            return first == name.npos ? std::string_view{}
                                      : name.substr(first, name.find_last_not_of(whitespace) - first + 1);
        }
        // Fold ASCII letters only. UTF-8 byte sequences remain unchanged.
        constexpr bool AsciiContains(std::string_view text, std::string_view query)
        {
            return !std::ranges::search(text, query,
                                        [](unsigned char x, unsigned char y) { return AsciiLower(x) == AsciiLower(y); })
                        .empty();
        }
        // Recognized vendor names match IDs (AMD also matches Radeon). Other queries match name substrings.
        constexpr bool AdapterNameMatches(std::string_view requested, std::string_view available, uint32_t vendorId = 0)
        {
            constexpr std::array<std::pair<std::string_view, uint32_t>, 3> vendors{
                {{"Nvidia", 0x10de}, {"AMD", 0x1002}, {"Intel", 0x8086}}};
            requested = TrimAdapterName(requested);
            if (requested.empty())
                return false;
            for (const auto& [name, id] : vendors)
                if (AsciiEqual(requested, name))
                    return vendorId == id;
            return AsciiContains(available, requested);
        }
    }  // namespace detail
}  // namespace OIV
