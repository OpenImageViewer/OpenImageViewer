#include <catch2/catch_all.hpp>

#include <Version.h>

TEST_CASE("OIViewer versions use Major.Minor.Patch.Revision order", "[version]")
{
    constexpr OIV::Version version{
        .major    = 1,
        .minor    = 2,
        .patch    = 3,
        .revision = 4,
    };
    constexpr auto expectedCurrentVersion = OIV::Version{
        .major    = OIV_VERSION_MAJOR,
        .minor    = OIV_VERSION_MINOR,
        .patch    = OIV_VERSION_PATCH,
        .revision = OIV_VERSION_REVISION,
    };

    REQUIRE(OIV::FormatReleaseVersion(version) == "1.2.3");
    REQUIRE(OIV::FormatFullVersion(version) == "1.2.3.4");
    REQUIRE(OIV::CurrentVersion == expectedCurrentVersion);
}
