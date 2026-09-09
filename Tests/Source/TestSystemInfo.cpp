#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <OIVAppCore/MessageHelper.h>
#include <LLUtils/StringUtility.h>
#include "NullRenderer.h"

namespace
{
    class SystemInfoRenderer : public OIV::NullRenderer
    {
      public:

        int index                      = -1;
        OIV::Acceleration acceleration = OIV::Acceleration::Unknown;

        const char* GetBackendName() const override { return "Vulkan"; }
        const char* GetGPUName() const override { return "Test GPU \xc3\xa9"; }
        const char* GetAPIVersion() const override { return "1.3"; }
        int GetSelectedGPUIndex() const override { return index; }
        OIV::Acceleration GetAcceleration() const override { return acceleration; }
    };
}  // namespace

TEST_CASE("System information formats adapter index and acceleration as adjacent rows", "[AppCore][system-info]")
{
    const int index = GENERATE(-1, 0, 7);
    SystemInfoRenderer renderer;
    renderer.index          = index;
    renderer.acceleration   = GENERATE(OIV::Acceleration::Hardware, OIV::Acceleration::Software,
                                       OIV::Acceleration::Unknown);
    const auto acceleration = LLUtils::StringUtility::ConvertString<LLUtils::native_string_type>(
        OIV::GetAccelerationName(renderer.acceleration));
    const auto text = OIV::MessageHelper::CreateSystemInfoMessage({
        .appName    = "OIViewer",
        .appVersion = "1.2.3.4",
        .gitHash    = "abcdef12",
        .buildType  = "Debug",
        .renderer   = &renderer,
    });
    for (const auto value : {LLUTILS_TEXT("OIViewer"), LLUTILS_TEXT("1.2.3.4"), LLUTILS_TEXT("abcdef12"),
                             LLUTILS_TEXT("Debug"), LLUTILS_TEXT("Vulkan"), LLUTILS_TEXT("Test GPU \u00e9"),
                             LLUTILS_TEXT("1.3"), LLUTILS_TEXT(" physical / "), LLUTILS_TEXT(" logical")})
        CHECK(text.find(value) != text.npos);
    for (const auto label :
         {LLUTILS_TEXT("Application"), LLUTILS_TEXT("Version"), LLUTILS_TEXT("Build"), LLUTILS_TEXT("Commit"),
          LLUTILS_TEXT("API version"), LLUTILS_TEXT("Driver version"), LLUTILS_TEXT("Not reported")})
        CHECK(text.find(label) != LLUtils::native_string_type::npos);
    const auto adapter         = text.find(LLUTILS_TEXT("Adapter"));
    const auto adapterIndex    = text.find(LLUTILS_TEXT("Adapter index"));
    const auto accelerationRow = text.find(LLUTILS_TEXT("Acceleration"));
    REQUIRE(adapter != text.npos);
    REQUIRE(adapterIndex != text.npos);
    REQUIRE(accelerationRow != text.npos);
    CHECK(adapter < adapterIndex);
    CHECK(adapterIndex < accelerationRow);
    const auto indexRow = text.substr(adapterIndex, accelerationRow - adapterIndex);
    CHECK(indexRow.find(LLUTILS_TEXT("...")) != text.npos);
    const auto expected = index < 0 ? LLUtils::native_string_type(LLUTILS_TEXT("Not reported"))
                                    : LLUtils::StringUtility::ConvertString<LLUtils::native_string_type>(
                                          std::to_string(index));
    CHECK(indexRow.find(expected) != text.npos);
    CHECK(text.find(acceleration, accelerationRow) != text.npos);
    CHECK(text.find(LLUTILS_TEXT("Adapter index:")) == text.npos);
}

TEST_CASE("System information supports an absent renderer", "[AppCore][system-info]")
{
    const auto text         = OIV::MessageHelper::CreateSystemInfoMessage({});
    const auto adapter      = text.find(LLUTILS_TEXT("Adapter"));
    const auto adapterIndex = text.find(LLUTILS_TEXT("Adapter index"));
    const auto acceleration = text.find(LLUTILS_TEXT("Acceleration"));
    const auto apiVersion   = text.find(LLUTILS_TEXT("API version"));
    REQUIRE(adapter != text.npos);
    REQUIRE(adapterIndex != text.npos);
    REQUIRE(acceleration != text.npos);
    REQUIRE(apiVersion != text.npos);
    CHECK(text.substr(adapter, adapterIndex - adapter).find(LLUTILS_TEXT("Unknown")) != text.npos);
    CHECK(text.substr(adapterIndex, acceleration - adapterIndex).find(LLUTILS_TEXT("Not reported")) != text.npos);
    CHECK(text.substr(acceleration, apiVersion - acceleration).find(LLUTILS_TEXT("Unknown")) != text.npos);
}
