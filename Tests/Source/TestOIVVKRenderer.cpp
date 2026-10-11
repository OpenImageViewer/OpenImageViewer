#if defined(OIV_TEST_VK_RENDERER)

    #include <catch2/catch_test_macros.hpp>

    #include <Image.h>
    #include <OIVVKRendererFactory.h>
    #include <RendererCore/Renderer.h>

    #include <stdexcept>
    #include <utility>

TEST_CASE("Vulkan renderer factory implements the current renderer contract", "[renderer][vulkan]")
{
    const OIV::RenderBackendSharedPtr renderer = OIV::VKRendererFactory::Create();

    REQUIRE(renderer != nullptr);
    OIV::Renderer facade(renderer);
    REQUIRE_NOTHROW(facade.CreateImage({}));
}

TEST_CASE("Vulkan renderer reports correct backend name", "[renderer][vulkan]")
{
    const OIV::RenderBackendSharedPtr renderer = OIV::VKRendererFactory::Create();
    REQUIRE(renderer != nullptr);
    REQUIRE(renderer->GetBackendName() != nullptr);
    REQUIRE(std::string(renderer->GetBackendName()) == "Vulkan");
}

TEST_CASE("Vulkan renderer accepts view parameters before init", "[renderer][vulkan]")
{
    const OIV::RenderBackendSharedPtr renderer = OIV::VKRendererFactory::Create();
    REQUIRE(renderer != nullptr);

    OIV::ViewParameters viewParams{};
    viewParams.uViewportSize = {64, 64};
    REQUIRE(renderer->SetViewParams(viewParams) == 0);
}

#endif
