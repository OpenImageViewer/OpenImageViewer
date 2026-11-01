#pragma once

#include <format>
#include <string>

inline constexpr int OIV_VERSION_MAJOR = 0;
inline constexpr int OIV_VERSION_MINOR = 18;
inline constexpr int OIV_VERSION_PATCH = 0;

/// Fallback for builds that do not provide Git-derived revision metadata.
#ifndef OIV_VERSION_REVISION
    #define OIV_VERSION_REVISION 0
#endif

namespace OIV
{
    struct Version
    {
        int major;
        int minor;
        int patch;
        int revision;

        constexpr bool operator==(const Version&) const = default;
    };

    inline constexpr Version CurrentVersion{
        .major    = OIV_VERSION_MAJOR,
        .minor    = OIV_VERSION_MINOR,
        .patch    = OIV_VERSION_PATCH,
        .revision = OIV_VERSION_REVISION,
    };

    static_assert(CurrentVersion.major >= 0);
    static_assert(CurrentVersion.minor >= 0);
    static_assert(CurrentVersion.patch >= 0);
    static_assert(CurrentVersion.revision >= 0);

    inline std::string FormatReleaseVersion(const Version& version)
    {
        return std::format("{}.{}.{}", version.major, version.minor, version.patch);
    }

    inline std::string FormatFullVersion(const Version& version)
    {
        return std::format("{}.{}", FormatReleaseVersion(version), version.revision);
    }
}  // namespace OIV
