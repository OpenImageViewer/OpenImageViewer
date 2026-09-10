#pragma once
#include <Interfaces/RendererOptions.h>

namespace OIV
{
    // Classify only recognized driver identities. Mesa or a translation layer alone is not
    // evidence of hardware acceleration; unknown drivers remain eligible in the middle tier.
    constexpr Acceleration ClassifyGLAcceleration(std::string_view vendor, std::string_view renderer)
    {
        constexpr std::array softwareNames{"llvmpipe",
                                           "softpipe",
                                           "swrast",
                                           "software rasterizer",
                                           "GDI Generic",
                                           "SwiftShader",
                                           "Microsoft Basic Render",
                                           "WARP"};
        for (const auto name : softwareNames)
            if (detail::AsciiContains(renderer, name))
                return Acceleration::Software;
        constexpr std::array hardwareVendors{"NVIDIA Corporation",    "Intel", "Intel Open Source Technology Center",
                                             "ATI Technologies Inc.", "AMD",   "Advanced Micro Devices, Inc."};
        for (const auto name : hardwareVendors)
            if (detail::AsciiEqual(vendor, name))
                return Acceleration::Hardware;
        return Acceleration::Unknown;
    }
}  // namespace OIV
