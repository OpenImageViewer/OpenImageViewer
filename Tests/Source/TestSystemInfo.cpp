#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <OIVAppCore/MessageHelper.h>
#include <LLUtils/StringUtility.h>
#include "NullRenderer.h"
#include <RendererCore/Renderer.h>

namespace
{
    class SystemInfoRenderer : public OIV::NullRenderer
    {
      public:

        OIV::RendererAdapter adapter{.name = "Test GPU \xc3\xa9", .apiVersion = "1.3"};

        mutable unsigned adapterReads = 0;
        const OIV::RendererAdapter& GetSelectedAdapter() const override
        {
            ++adapterReads;
            return adapter;
        }
    };
}  // namespace

TEST_CASE("System information formats adapter index and acceleration as adjacent rows", "[AppCore][system-info]")
{
    const int index = GENERATE(-1, 0, 7);
    auto backend    = std::make_unique<SystemInfoRenderer>();
    auto& renderer  = *backend;
    OIV::Renderer facade(std::move(backend));
    renderer.adapter.index        = index;
    renderer.adapter.acceleration = GENERATE(OIV::Acceleration::Hardware, OIV::Acceleration::Software,
                                             OIV::Acceleration::Unknown);
    const auto acceleration       = LLUtils::StringUtility::ConvertString<LLUtils::native_string_type>(
        OIV::GetAccelerationName(renderer.adapter.acceleration));
    const auto text = OIV::MessageHelper::CreateSystemInfoMessage({
        .appName      = "OIViewer",
        .appVersion   = "1.2.3.4",
        .gitHash      = "abcdef12",
        .buildType    = "Debug",
        .renderer     = &facade,
        .rendererType = OIV::RendererType::Vulkan,
    });
    CHECK(renderer.adapterReads == 1);
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

TEST_CASE("Null renderer exposes stable empty adapter and version records", "[AppCore][system-info]")
{
    OIV::NullRenderer renderer;
    const auto& adapter = renderer.GetSelectedAdapter();
    renderer.Init({});
    CHECK(renderer.EnumerateAdapters().empty());
    CHECK(&renderer.GetSelectedAdapter() == &adapter);
    CHECK(adapter.index == -1);
    CHECK(adapter.name.empty());
    CHECK(adapter.acceleration == OIV::Acceleration::Unknown);
    CHECK_FALSE(adapter.preferred);
    CHECK(adapter.apiVersion.empty());
    CHECK(adapter.driverVersion.empty());
}

TEST_CASE("System information derives backend names from the selected enum", "[AppCore][system-info]")
{
    const auto [type, name]        = GENERATE(std::pair{OIV::RendererType::D3D11, "D3D11"},
                                              std::pair{OIV::RendererType::Vulkan, "Vulkan"},
                                              std::pair{OIV::RendererType::OpenGL, "GL"},
                                              std::pair{OIV::RendererType::Null, "Null"});
    auto backend                   = std::make_unique<SystemInfoRenderer>();
    backend->adapter.driverVersion = "123.456";
    OIV::Renderer renderer(std::move(backend));
    const auto text = OIV::MessageHelper::CreateSystemInfoMessage({.renderer = &renderer, .rendererType = type});
    const auto row  = text.find(LLUTILS_TEXT("Renderer"));
    REQUIRE(row != text.npos);
    const auto rendererRow = text.substr(row, text.find(LLUTILS_TEXT("Adapter"), row) - row);
    CHECK(rendererRow.find(LLUtils::StringUtility::ConvertString<LLUtils::native_string_type>(name)) != text.npos);
    CHECK(text.find(LLUTILS_TEXT("123.456")) != text.npos);
}
