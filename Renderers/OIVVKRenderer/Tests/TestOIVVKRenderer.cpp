#if defined(OIV_TEST_VK_RENDERER)

    #include <catch2/catch_test_macros.hpp>

    #include <Image.h>
    #include <OIVVKRendererFactory.h>
    #include <RendererCore/Renderer.h>

    #include <stdexcept>
    #include <utility>

TEST_CASE("Vulkan renderer factory implements the current renderer contract", "[renderer][vulkan]")
{
    auto renderer = OIV::CreateVulkanBackend();

    REQUIRE(renderer != nullptr);
    OIV::Renderer facade(std::move(renderer));
    REQUIRE_NOTHROW(facade.CreateImage({}));
}

TEST_CASE("Vulkan renderer exposes an empty selected adapter before initialization", "[renderer][vulkan]")
{
    auto renderer = OIV::CreateVulkanBackend();
    REQUIRE(renderer != nullptr);
    CHECK(renderer->GetSelectedAdapter().index == -1);
    CHECK(renderer->GetSelectedAdapter().apiVersion.empty());
}

TEST_CASE("Vulkan renderer accepts view parameters before init", "[renderer][vulkan]")
{
    auto renderer = OIV::CreateVulkanBackend();
    REQUIRE(renderer != nullptr);

    OIV::ViewParameters viewParams{};
    viewParams.uViewportSize = {64, 64};
    REQUIRE_NOTHROW(renderer->SetViewParams(viewParams));
}

#endif
