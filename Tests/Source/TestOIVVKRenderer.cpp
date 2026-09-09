#if defined(OIV_TEST_VK_RENDERER)

    #include <catch2/catch_test_macros.hpp>

    #include <Image.h>
    #include <OIVVKRendererFactory.h>

    #include <stdexcept>
    #include <utility>

namespace
{
    class TestRenderable final : public OIV::IRenderable
    {
      public:

        explicit TestRenderable(IMCodec::ImageSharedPtr image = {}) : fImage(std::move(image)) {}

        double GetOpacity() const override { return 1.0; }
        LLUtils::PointF64 GetScale() const override { return {1.0, 1.0}; }
        LLUtils::PointF64 GetPosition() const override { return {}; }
        IMCodec::ImageSharedPtr GetImage() override { return fImage; }
        OIV_Filter_type GetFilterType() const override { return FT_Linear; }
        bool GetVisible() const override { return true; }
        uint32_t GetID() const override { return 1; }
        OIV_Image_Render_mode GetImageRenderMode() const override { return IRM_MainImage; }
        bool GetIsImageDirty() const override { return fImageDirty; }
        void ClearImageDirty() override { fImageDirty = false; }
        void PreRender() override {}

      private:

        IMCodec::ImageSharedPtr fImage;
        bool fImageDirty = fImage != nullptr;
    };
}  // namespace

TEST_CASE("Vulkan renderer factory implements the current renderer contract", "[renderer][vulkan]")
{
    const OIV::IRendererSharedPtr renderer = OIV::VKRendererFactory::Create();
    TestRenderable renderable;

    REQUIRE(renderer != nullptr);
    REQUIRE_THROWS_AS(renderer->AddRenderable(nullptr), std::invalid_argument);
    REQUIRE(renderer->AddRenderable(&renderable) == 0);
    REQUIRE(renderer->RemoveRenderable(&renderable) == 0);
}

TEST_CASE("Vulkan renderer reports correct backend name", "[renderer][vulkan]")
{
    const OIV::IRendererSharedPtr renderer = OIV::VKRendererFactory::Create();
    REQUIRE(renderer != nullptr);
    REQUIRE(renderer->GetBackendName() != nullptr);
    REQUIRE(std::string(renderer->GetBackendName()) == "Vulkan");
}

TEST_CASE("Vulkan renderer accepts view parameters before init", "[renderer][vulkan]")
{
    const OIV::IRendererSharedPtr renderer = OIV::VKRendererFactory::Create();
    REQUIRE(renderer != nullptr);

    OIV::ViewParameters viewParams{};
    viewParams.uViewportSize = {64, 64};
    REQUIRE(renderer->SetViewParams(viewParams) == 0);
}

#endif
