#include <windows.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "VKRuntime.h"
#include "RendererSelection.h"
#include "CommandLine.h"
#include <filesystem>
#include <LLUtils/PlatformUtility.h>

TEST_CASE("Help and version do not initialize the missing Vulkan runtime", "[vulkan][loader][cli]")
{
    for (const auto argument : {L"--help", L"--version"})
    {
        const wchar_t* args[]{L"OIViewer", argument};
        const auto parsed = OIV::ParseCommandLine(2, args);
        const auto result = std::get<OIV::CommandLineExit>(parsed);
        CHECK(result.exitCode == 0);
        CHECK_FALSE(result.standardOutput.empty());
        CHECK(OIV::vkCreateInstance == nullptr);
    }
}
TEST_CASE("Missing Vulkan runtime is cached and explicit selection fails", "[vulkan][loader]")
{
    for (int i = 0; i < 2; ++i)
        CHECK_THROWS_WITH(OIV::EnsureVulkanRuntime(),
                          Catch::Matchers::ContainsSubstring("Vulkan runtime could not be loaded"));
    CHECK_THROWS_WITH(OIV::SelectRenderer(OIV::GetRendererBackends(), {.renderer = OIV::RendererType::Vulkan}, {}),
                      Catch::Matchers::ContainsSubstring("Vulkan runtime could not be loaded"));
    CHECK_THROWS_WITH(OIV::SelectRenderer(OIV::GetRendererBackends(), {.adapterIndex = 0}, {}),
                      Catch::Matchers::ContainsSubstring("Vulkan runtime could not be loaded"));
}
TEST_CASE("Missing Vulkan runtime permits real D3D11 startup", "[vulkan][loader][d3d11]")
{
    if (!OIV::IsRendererAvailable(OIV::RendererType::D3D11))
        SKIP("D3D11 is not compiled into this configuration");
    struct Window
    {
        HWND handle = CreateWindowExW(0, L"STATIC", L"Renderer loader test", WS_OVERLAPPEDWINDOW, 0, 0, 640, 480,
                                      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        ~Window()
        {
            if (handle)
                DestroyWindow(handle);
        }
    } window;
    REQUIRE(window.handle != nullptr);
    const auto path =
        (std::filesystem::path(LLUtils::PlatformUtility::GetExeFolder()) / "runtime-failure-cache").wstring();
    const OIV_RendererInitializationParams params{
        .container = reinterpret_cast<size_t>(window.handle),
        .dataPath  = path.c_str(),
    };
    const auto renderer = OIV::SelectRenderer(OIV::GetRendererBackends(), {}, params);
    CHECK(std::string(renderer->GetBackendName()) == "D3D11");
    CHECK(renderer->GetAcceleration() != OIV::Acceleration::Unknown);
}
