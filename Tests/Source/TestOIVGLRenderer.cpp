#if defined(OIV_TEST_GL_RENDERER)

    #include <catch2/catch_test_macros.hpp>

    #include <Image.h>
    #include <OIVGLRendererFactory.h>

    #if defined(LWS_HAS_WAYLAND_BACKEND)
        #include <GL/glew.h>
        #include <LWS/Platform.hpp>
        #include <LWS/Wayland/WindowExtensions.hpp>
        #include <LWS/Window.hpp>
    #endif

    #include <algorithm>
    #include <array>
    #include <cstdlib>
    #include <utility>

namespace
{
    class TestRenderable final : public OIV::IRenderable
    {
      public:

        explicit TestRenderable(IMCodec::ImageSharedPtr image = {}, OIV_Image_Render_mode mode = IRM_MainImage,
                                uint32_t id = 1)
            : fImage(std::move(image)), fMode(mode), fId(id)
        {
        }

        double GetOpacity() const override { return 1.0; }
        LLUtils::PointF64 GetScale() const override { return {1.0, 1.0}; }
        LLUtils::PointF64 GetPosition() const override { return {}; }
        IMCodec::ImageSharedPtr GetImage() override { return fImage; }
        OIV_Filter_type GetFilterType() const override { return FT_Linear; }
        bool GetVisible() const override { return true; }
        uint32_t GetID() const override { return fId; }
        OIV_Image_Render_mode GetImageRenderMode() const override { return fMode; }
        bool GetIsImageDirty() const override { return fImageDirty; }
        void ClearImageDirty() override { fImageDirty = false; }
        void PreRender() override {}

      private:

        IMCodec::ImageSharedPtr fImage;
        OIV_Image_Render_mode fMode;
        uint32_t fId;
        bool fImageDirty = fImage != nullptr;
    };

    IMCodec::ImageSharedPtr CreateBlackImage(uint8_t alpha = 0, uint32_t size = 1)
    {
        auto imageItem                                = std::make_shared<IMCodec::ImageItem>();
        imageItem->itemType                           = IMCodec::ImageItemType::Image;
        imageItem->descriptor.width                   = size;
        imageItem->descriptor.height                  = size;
        imageItem->descriptor.rowPitchInBytes         = size * 4;
        imageItem->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
        imageItem->descriptor.texelFormatStorage      = IMCodec::TexelFormat::I_R8_G8_B8_A8;
        imageItem->data.Allocate(size * size * 4);
        std::fill_n(imageItem->data.data(), imageItem->data.size(), std::byte{});
        for (size_t offset = 3; offset < imageItem->data.size(); offset += 4)
            imageItem->data.data()[offset] = static_cast<std::byte>(alpha);
        return std::make_shared<IMCodec::Image>(imageItem, IMCodec::ImageItemType::Unknown);
    }

    #if defined(LWS_HAS_WAYLAND_BACKEND)
    std::array<uint8_t, 4> ReadPixel(int32_t x, int32_t y)
    {
        std::array<uint8_t, 4> pixel{};
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(x, y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
        return pixel;
    }
    #endif
}  // namespace

TEST_CASE("OpenGL renderer factory implements the current renderer contract", "[renderer][opengl]")
{
    const OIV::IRendererSharedPtr renderer = OIV::GLRendererFactory::Create();
    TestRenderable renderable;

    REQUIRE(renderer != nullptr);
    REQUIRE(renderer->AddRenderable(&renderable) == 0);
    REQUIRE(renderer->RemoveRenderable(&renderable) == 0);
}

    #if defined(LWS_HAS_WAYLAND_BACKEND)
TEST_CASE("OpenGL renderer draws canvas, selection, and overlays with opaque output", "[renderer][opengl][wayland]")
{
    LWS::PlatformContext platform;
    if (platform.Init({.backend = LWS::BackendId::Wayland}) != LWS::Result::Success)
        SKIP("No Wayland compositor is available");

    LWS::Window window(platform);
    const LWS::WindowConfig windowConfig{
        .clientSize      = {64, 64},
        .visible         = true,
        .eraseBackground = false,
    };
    REQUIRE(window.Create(windowConfig) == LWS::Result::Success);
    std::ignore = platform.RefreshMonitors();

    const OIV::IRendererSharedPtr renderer = OIV::GLRendererFactory::Create();
    REQUIRE(renderer->Init({
                .container     = reinterpret_cast<LWS::Handle>(*LWS::Wayland::GetSurface(window)),
                .nativeDisplay = *LWS::Wayland::GetDisplay(window),
            }) == 0);

    GLuint framebuffer{};
    GLuint colorTexture{};
    glGenFramebuffers(1, &framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glGenTextures(1, &colorTexture);
    glBindTexture(GL_TEXTURE_2D, colorTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexture, 0);
    REQUIRE(glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE);

    REQUIRE(renderer->SetViewParams({
                .uViewportSize       = {64, 64},
                .uTransparencyColor1 = {0, 255, 0},
                .uTransparencyColor2 = {255, 255, 0},
                .showGrid            = false,
            }) == 0);
    REQUIRE(renderer->Redraw() == 0);
    REQUIRE((ReadPixel(0, 0) == std::array<uint8_t, 4>{45, 45, 48, 255}));

    REQUIRE(renderer->SetBackgroundColor(0, {255, 0, 0}) == 0);
    REQUIRE(renderer->SetBackgroundColor(1, {0, 0, 255}) == 0);
    TestRenderable image(CreateBlackImage());
    REQUIRE(renderer->AddRenderable(&image) == 0);
    REQUIRE(renderer->Redraw() == 0);
    const auto backgroundFirst         = ReadPixel(8, 8);
    const auto backgroundSecond        = ReadPixel(24, 8);
    const auto selectionInteriorBefore = ReadPixel(32, 32);
    const auto selectionBorderBefore   = ReadPixel(16, 32);
    REQUIRE(glGetError() == GL_NO_ERROR);
    REQUIRE(backgroundFirst[0] > 200);
    REQUIRE(backgroundFirst[2] < 50);
    REQUIRE(backgroundSecond[0] < 50);
    REQUIRE(backgroundSecond[2] > 200);

    REQUIRE(renderer->SetSelectionRect({{16, 16}, {48, 48}}) == 0);
    REQUIRE(renderer->Redraw() == 0);
    REQUIRE(ReadPixel(8, 8) != backgroundFirst);
    REQUIRE(ReadPixel(32, 32) == selectionInteriorBefore);
    REQUIRE(ReadPixel(16, 32) != selectionBorderBefore);

    REQUIRE(renderer->SetSelectionRect({{-8, 16}, {48, 48}}) == 0);
    REQUIRE(renderer->Redraw() == 0);
    REQUIRE(ReadPixel(8, 8) != backgroundFirst);

    REQUIRE(renderer->SetSelectionRect({{-1, -1}, {-1, -1}}) == 0);
    // Image info uses alpha 127; F1 and system info use 216. Check RGB blending and
    // final window alpha independently, including redraws and transparent/opaque endpoints.
    for (const uint8_t alpha : {0, 127, 216, 255})
    {
        CAPTURE(alpha);
        TestRenderable overlay(CreateBlackImage(alpha, 64), IRM_Overlay, 2);
        REQUIRE(renderer->AddRenderable(&overlay) == 0);
        for (int frame = 0; frame < 2; ++frame)
        {
            REQUIRE(renderer->Redraw() == 0);
            const auto first   = ReadPixel(8, 8);
            const auto second  = ReadPixel(24, 8);
            const int expected = 255 - alpha;
            CHECK(std::abs(static_cast<int>(first[0]) - expected) <= 1);
            CHECK(first[1] == 0);
            CHECK(first[2] == 0);
            CHECK(first[3] == 255);
            CHECK(second[0] == 0);
            CHECK(second[1] == 0);
            CHECK(std::abs(static_cast<int>(second[2]) - expected) <= 1);
            CHECK(second[3] == 255);
        }
        REQUIRE(renderer->RemoveRenderable(&overlay) == 0);
    }
    REQUIRE(glGetError() == GL_NO_ERROR);
    REQUIRE(renderer->RemoveRenderable(&image) == 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteTextures(1, &colorTexture);
    glDeleteFramebuffers(1, &framebuffer);
}
    #endif

#endif
