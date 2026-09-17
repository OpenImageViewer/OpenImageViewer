#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "ViewerRenderPort.h"
#include "ApiGlobal.h"
#include "OIV.h"
#include <OIVImage/OIVBaseImage.h>
#include <OIVImage/OIVFileImage.h>
#include <OIVImage/OIVTextImage.h>
#include <filesystem>
#include <fstream>
#include <future>
#include <array>
#include <iterator>
#include <type_traits>
#include <latch>
#include <thread>
#include <LWS/Platform.hpp>
#include <LWS/Window.hpp>
#ifdef LWS_HAS_WIN32_BACKEND
    #include <LWS/Win32/Platform.hpp>
    #include <LWS/Win32/WindowExtensions.hpp>
#elif defined(LWS_HAS_WAYLAND_BACKEND)
    #include <LWS/Wayland/WindowExtensions.hpp>
#endif

namespace
{
    static_assert(!std::is_copy_constructible_v<OIV::OIV>);
    static_assert(!std::is_copy_assignable_v<OIV::OIV>);
    static_assert(!std::is_move_constructible_v<OIV::OIV>);
    static_assert(!std::is_move_assignable_v<OIV::OIV>);
    static_assert(!std::is_copy_constructible_v<OIV::OIVBaseImage>);
    static_assert(!std::is_copy_assignable_v<OIV::OIVBaseImage>);
    static_assert(!std::is_move_constructible_v<OIV::OIVBaseImage>);
    static_assert(!std::is_move_assignable_v<OIV::OIVBaseImage>);

    // Each test owns its API instance without disturbing the suite's other image tests.
    struct ScopedApi
    {
        std::unique_ptr<OIV::IPictureRenderer> previous = std::move(OIV::ApiGlobal::sPictureRenderer);

        ScopedApi() { OIV::ApiGlobal::sPictureRenderer = std::make_unique<OIV::OIV>(); }
        ~ScopedApi() { OIV::ApiGlobal::sPictureRenderer = std::move(previous); }
    };
}  // namespace

TEST_CASE("Render gateway releases the API after images on success or initialization failure", "[renderer][lifetime]")
{
    ScopedApi api;
    const bool fail = GENERATE(false, true);
    const OIV::RendererOptions options{.renderer = fail ? static_cast<OIV::RendererType>(-1) : OIV::RendererType::Null};
    {
        OIV::OivRenderGateway gateway;
        OIV::OIVBaseImage image(OIV::ImageSource::GeneratedByLib);
        if (fail)
            REQUIRE_THROWS_WITH(gateway.Initialize(0, nullptr, options),
                                Catch::Matchers::ContainsSubstring("not available"));
        else
        {
            REQUIRE_NOTHROW(gateway.Initialize(0, nullptr, options));
            REQUIRE(std::string(OIV::ApiGlobal::sPictureRenderer->GetRenderer()->GetBackendName()) == "Null");
        }
        // The image's destructor needs the API, so it runs before the gateway releases it.
        REQUIRE(OIV::ApiGlobal::sPictureRenderer != nullptr);
    }
    CHECK(OIV::ApiGlobal::sPictureRenderer == nullptr);
}

TEST_CASE("Unused render gateway leaves the API available", "[renderer][lifetime]")
{
    ScopedApi api;
    {
        OIV::OivRenderGateway gateway;
    }
    CHECK(OIV::ApiGlobal::sPictureRenderer != nullptr);
}

TEST_CASE("Renderer shutdown disconnects its global exception subscription", "[renderer][lifetime]")
{
    ScopedApi api;
    const OIV::RendererOptions options{.renderer = OIV::RendererType::Null};
    int callbacks = 0;
    {
        OIV::OivRenderGateway gateway;
        REQUIRE_NOTHROW(gateway.Initialize(0, nullptr, options));
        const OIV_CMD_RegisterCallbacks_Request callback{.OnException = [](OIV_Exception_Args, void* userPointer)
                                                         { ++*static_cast<int*>(userPointer); },
                                                         .userPointer = &callbacks};
        REQUIRE(OIV::ApiGlobal::sPictureRenderer->RegisterCallbacks(callback) == RC_Success);
        LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
        REQUIRE(callbacks == 1);
    }
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(callbacks == 1);
}

TEST_CASE("Render gateway forwards zero extents and restores the same viewport", "[renderer][lifetime]")
{
    ScopedApi api;
    const OIV::RendererOptions options{.renderer = OIV::RendererType::Null};
    OIV::OivRenderGateway gateway;
    gateway.Initialize(0, nullptr, options);
    const auto& renderer = static_cast<const OIV::OIV&>(*OIV::ApiGlobal::sPictureRenderer);

    for (const auto size : {LWS::PixelSize{}, LWS::PixelSize{640, 480}, LWS::PixelSize{}, LWS::PixelSize{640, 480},
                            LWS::PixelSize{640, 480}})
    {
        REQUIRE(gateway.SetViewportSize(size) == RC_Success);
        REQUIRE(renderer.GetClientSize() == LLUtils::PointI32{size.x, size.y});
    }
}

TEST_CASE("File decoding finishes without an OIV instance before renderer initialization",
          "[renderer][lifetime][loading]")
{
    ScopedApi api;
    OIV::ApiGlobal::sPictureRenderer.reset();
    IMCodec::ImageLoader loader;
    if (loader.GetFirstPlugin(LLUTILS_TEXT("jpg")) == IMCodec::PluginID{})
        SKIP("JPEG decoding is not configured");
    const auto fixture = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                         "External/ImageCodec/Example/cat.jpg";
    IMCodec::ItemMetaDataSharedPtr metaData;
    auto future = std::async(
        std::launch::async, [&]
        { return OIV::DecodeFileImage(loader, fixture.native(), metaData, IMCodec::PluginTraverseMode::NoTraverse); });
    auto decoded = future.get();
    REQUIRE(decoded != nullptr);
    REQUIRE(metaData != nullptr);
    REQUIRE(OIV::ApiGlobal::sPictureRenderer == nullptr);
    const auto* pixels = decoded->GetBufferAt(0, 0);

    OIV::ApiGlobal::sPictureRenderer = std::make_unique<OIV::OIV>();
    OIV::OivRenderGateway gateway;
    SECTION("Successful initialization wraps the existing pixels on the calling thread")
    {
        REQUIRE_NOTHROW(gateway.Initialize(0, nullptr, {.renderer = OIV::RendererType::Null}));
        OIV::OIVFileImage image(fixture.native(), std::move(decoded));
        image.SetMetaData(std::move(metaData));
        CHECK(image.GetFileName() == fixture.native());
        CHECK(image.GetImageSource() == OIV::ImageSource::File);
        CHECK(image.GetImage()->GetBufferAt(0, 0) == pixels);
    }
    SECTION("Initialization failure leaves data safe to release without a renderer")
    {
        REQUIRE_THROWS(gateway.Initialize(0, nullptr, {.renderer = static_cast<OIV::RendererType>(-1)}));
        OIV::ApiGlobal::sPictureRenderer.reset();
        decoded.reset();
        metaData.reset();
        SUCCEED("Decoded data has no renderer registration to release");
    }
}

TEST_CASE("Decoded file data preserves EXIF orientation and failed-load behavior", "[renderer][lifetime][loading]")
{
    ScopedApi api;
    IMCodec::ImageLoader loader;
    if (loader.GetFirstPlugin(LLUTILS_TEXT("jpg")) == IMCodec::PluginID{})
        SKIP("JPEG decoding is not configured");
    const auto fixture = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                         "External/ImageCodec/Example/cat.jpg";
    IMCodec::ImageSharedPtr original;
    REQUIRE(loader.Decode(fixture.native(), IMCodec::ImageLoadFlags::None, {}, IMCodec::PluginTraverseMode::NoTraverse,
                          original) == IMCodec::ImageResult::Success);
    REQUIRE(original != nullptr);

    struct TemporaryDirectory
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
                                     ("oiv-decoded-image-" +
                                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TemporaryDirectory() { std::filesystem::create_directory(path); }
        ~TemporaryDirectory()
        {
            std::error_code error;
            std::filesystem::remove_all(path, error);
        }
    } temporary;
    const auto oriented = temporary.path / "oriented.jpg";
    // Change the fixture's existing big-endian EXIF orientation entry to 6 (90 degrees
    // clockwise). Verify the entry first so a fixture replacement cannot silently weaken the test.
    {
        std::ifstream input(fixture, std::ios::binary);
        std::string bytes(std::istreambuf_iterator<char>{input}, {});
        const std::array<char, 12> orientationEntry{1, 0x12, 0, 3, 0, 0, 0, 1, 0, 1, 0, 0};
        const auto entry = bytes.find(std::string_view(orientationEntry.data(), orientationEntry.size()));
        REQUIRE(entry != std::string::npos);
        bytes[entry + 9] = 6;
        std::ofstream output(oriented, std::ios::binary);
        output.write(bytes.data(), bytes.size());
        REQUIRE(output.good());
    }
    IMCodec::ItemMetaDataSharedPtr metaData;
    const auto decoded = OIV::DecodeFileImage(loader, oriented.native(), metaData,
                                              IMCodec::PluginTraverseMode::NoTraverse);
    REQUIRE(decoded != nullptr);
    REQUIRE(metaData != nullptr);
    CHECK(metaData->exifData.orientation == 6);
    CHECK(decoded->GetWidth() == original->GetHeight());
    CHECK(decoded->GetHeight() == original->GetWidth());

    OIV::OIVFileImage file(oriented.native());
    REQUIRE(file.Load(&loader, IMCodec::PluginTraverseMode::NoTraverse) == RC_Success);
    CHECK(file.GetImage()->GetWidth() == decoded->GetWidth());
    CHECK(file.GetMetaData()->exifData.orientation == metaData->exifData.orientation);

    const auto unsupported = temporary.path / "unsupported.txt";
    {
        std::ofstream output(unsupported);
        output << "not an image";
    }
    // Reusing the output must not retain metadata from the previous successful decode.
    CHECK(OIV::DecodeFileImage(loader, unsupported.native(), metaData, IMCodec::PluginTraverseMode::NoTraverse) ==
          nullptr);
    CHECK(metaData == nullptr);
    OIV::OIVFileImage failed(unsupported.native());
    CHECK(failed.Load(&loader, IMCodec::PluginTraverseMode::NoTraverse) == RC_FileNotSupported);
}

TEST_CASE("Rendered text becomes clean and reuses its bitmap until content changes", "[renderer][text]")
{
    ScopedApi api;
    FreeType::FreeTypeConnector connector;
    OIV::OIVTextImage text(&connector);
    const auto font = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                      "Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf";
    text.SetFontPath(font.native());
    text.SetFontSize(24);
    text.SetDPI(96, 96);
    text.SetText(LLUTILS_TEXT("OIV"));
    CHECK(text.IsDirty());
    const auto metrics = text.GetMetrics();
    CHECK(text.IsDirty());  // Measuring still leaves the bitmap pending.
    text.PreRender();
    REQUIRE(text.GetImage() != nullptr);
    CHECK_FALSE(text.IsDirty());
    CHECK(metrics.pixelSize.x == text.GetImage()->GetWidth());
    CHECK(metrics.pixelSize.y == text.GetImage()->GetHeight());
    const auto bitmap = text.GetImage();

    text.SetFontSize(24);
    text.SetDPI(96, 96);
    text.SetText(LLUTILS_TEXT("OIV"));
    CHECK_FALSE(text.IsDirty());
    text.SetPosition({10, 20});
    CHECK(text.IsDirty());
    text.PreRender();
    CHECK_FALSE(text.IsDirty());
    CHECK(text.GetImage() == bitmap);

    text.SetFontSize(14);
    CHECK(text.IsDirty());
    text.GetMetrics();
    CHECK(text.IsDirty());
    text.PreRender();
    CHECK_FALSE(text.IsDirty());
    CHECK(text.GetImage() != bitmap);
}

TEST_CASE("Explicit renderer shutdown preserves the API until surviving images are released", "[renderer][lifetime]")
{
    ScopedApi api;
    const bool fail = GENERATE(false, true);
    {
        OIV::OivRenderGateway gateway;
        auto image = std::make_unique<OIV::OIVBaseImage>(OIV::ImageSource::GeneratedByLib);
        if (fail)
            REQUIRE_THROWS(gateway.Initialize(0, nullptr, {.renderer = static_cast<OIV::RendererType>(-1)}));
        else
            REQUIRE_NOTHROW(gateway.Initialize(0, nullptr, {.renderer = OIV::RendererType::Null}));

        auto* renderer = OIV::ApiGlobal::sPictureRenderer.get();
        renderer->Shutdown();
        renderer->Shutdown();
        CHECK(renderer->GetRenderer() == nullptr);
        REQUIRE(OIV::ApiGlobal::sPictureRenderer.get() == renderer);
        image.reset();

        // The API remains usable for registration and another initialization after native shutdown.
        OIV::OIVBaseImage laterImage(OIV::ImageSource::GeneratedByLib);
        REQUIRE(renderer->Init({.renderer = OIV::RendererType::Null}) == 0);
        REQUIRE(renderer->GetRenderer() != nullptr);
        renderer->Shutdown();
    }
    CHECK(OIV::ApiGlobal::sPictureRenderer == nullptr);
}

TEST_CASE("Explicit renderer shutdown disconnects callbacks before reinitialization", "[renderer][lifetime]")
{
    ScopedApi api;
    auto& renderer         = *OIV::ApiGlobal::sPictureRenderer;
    unsigned notifications = 0;
    const OIV_CMD_RegisterCallbacks_Request callbacks{
        .OnException = [](OIV_Exception_Args, void* user) { ++*static_cast<unsigned*>(user); },
        .userPointer = &notifications,
    };
    REQUIRE(renderer.Init({.renderer = OIV::RendererType::Null}) == 0);
    REQUIRE(renderer.RegisterCallbacks(callbacks) == RC_Success);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    REQUIRE(notifications == 1);
    renderer.Shutdown();
    REQUIRE(renderer.RegisterCallbacks(callbacks) == RC_Success);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(notifications == 1);
    REQUIRE(renderer.Init({.renderer = OIV::RendererType::Null}) == 0);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(notifications == 2);
    renderer.Shutdown();
}

TEST_CASE("Renderer exception callbacks can be registered and replaced across initialization", "[renderer][lifetime]")
{
    ScopedApi api;
    auto& renderer                = *OIV::ApiGlobal::sPictureRenderer;
    const bool registerBeforeInit = GENERATE(false, true);
    unsigned first                = 0;
    unsigned second               = 0;
    OIV_CMD_RegisterCallbacks_Request callbacks{
        .OnException = [](OIV_Exception_Args, void* user) { ++*static_cast<unsigned*>(user); },
        .userPointer = &first,
    };
    if (registerBeforeInit)
        REQUIRE(renderer.RegisterCallbacks(callbacks) == RC_Success);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(first == 0);
    REQUIRE(renderer.Init({.renderer = OIV::RendererType::Null}) == 0);
    if (!registerBeforeInit)
        REQUIRE(renderer.RegisterCallbacks(callbacks) == RC_Success);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(first == 1);

    callbacks.userPointer = &second;
    REQUIRE(renderer.RegisterCallbacks(callbacks) == RC_Success);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(first == 1);
    CHECK(second == 1);
    REQUIRE(renderer.RegisterCallbacks({}) == RC_Success);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(second == 1);
    REQUIRE(renderer.RegisterCallbacks(callbacks) == RC_Success);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(second == 2);

    renderer.Shutdown();
    callbacks.userPointer = &first;
    REQUIRE(renderer.RegisterCallbacks(callbacks) == RC_Success);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(first == 1);
    REQUIRE(renderer.Init({.renderer = OIV::RendererType::Null}) == 0);
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(first == 2);
    CHECK(second == 2);
    renderer.Shutdown();
    renderer.Shutdown();
    LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
    CHECK(first == 2);
}

TEST_CASE("Renderer without exception callbacks leaves worker reporting independent of shutdown",
          "[renderer][lifetime]")
{
    ScopedApi api;
    auto& renderer         = *OIV::ApiGlobal::sPictureRenderer;
    unsigned notifications = 0;
    auto observer = LLUtils::Exception::OnException.Connect([&](LLUtils::Exception::EventArgs) { ++notifications; });
    std::latch started(1);
    std::jthread worker(
        [&](std::stop_token stop)
        {
            LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
            started.count_down();
            while (!stop.stop_requested())
                LLUtils::Exception::OnException.Raise(LLUtils::Exception::EventArgs{});
        });
    started.wait();
    // The viewer does not request the API bridge. Its renderer lifecycle must leave the
    // single-threaded global event alone while a decoder worker reports exceptions.
    constexpr unsigned lifecycleCount = 1000;
    for (unsigned iteration = 0; iteration < lifecycleCount; ++iteration)
    {
        REQUIRE(renderer.Init({.renderer = OIV::RendererType::Null}) == 0);
        renderer.Shutdown();
    }
    worker.request_stop();
    worker.join();
    CHECK(notifications > 0);
}

// Run explicitly with a native window backend and an available renderer.
TEST_CASE("Renderer shutdown releases native state and exception registrations", "[.][renderer][native-lifecycle]")
{
    ScopedApi api;
    LWS::PlatformContext platform;
#ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
#else
    REQUIRE(platform.Init({.backend = LWS::BackendId::Wayland}) == LWS::Result::Success);
#endif
    LWS::Window window(platform);
    REQUIRE(window.Create() == LWS::Result::Success);
    auto& renderer = *OIV::ApiGlobal::sPictureRenderer;
    struct Cleanup
    {
        OIV::IPictureRenderer& renderer;
        ~Cleanup() { renderer.Shutdown(); }
    } cleanup{renderer};
    void* nativeDisplay = nullptr;
#ifdef LWS_HAS_WIN32_BACKEND
    const auto handle = LWS::Win32::GetHwnd(window);
    REQUIRE(handle);
#else
    const auto handle  = LWS::Wayland::GetSurface(window);
    const auto display = LWS::Wayland::GetDisplay(window);
    REQUIRE(handle);
    REQUIRE(display);
    nativeDisplay = *display;
#endif
    REQUIRE(renderer.SetParent(reinterpret_cast<std::size_t>(*handle), nativeDisplay) == 0);
    REQUIRE(renderer.Init() == 0);
    REQUIRE(renderer.GetRenderer() != nullptr);
    SECTION("Native cleanup precedes window destruction and allows later image release")
    {
        auto image    = std::make_shared<OIV::OIVBaseImage>(OIV::ImageSource::GeneratedByLib);
        bool released = false;
        auto listener = window.Listen(
            [&](const LWS::AnyEvent& event)
            {
                if (std::holds_alternative<LWS::EventWindowDestroying>(event))
                {
                    renderer.Shutdown();
                    released = renderer.GetRenderer() == nullptr;
                }
                return LWS::EventResponse::Unhandled;
            });
        REQUIRE(listener);
        REQUIRE(window.Destroy() == LWS::Result::Success);
        REQUIRE(released);
        image.reset();
        REQUIRE(renderer.GetRenderer() == nullptr);
    }
    SECTION("Exception forwarding is disconnected and can be reinitialized")
    {
        unsigned notifications = 0;
        const OIV_CMD_RegisterCallbacks_Request callbacks{
            .OnException = [](OIV_Exception_Args, void* user) { ++*static_cast<unsigned*>(user); },
            .userPointer = &notifications,
        };
        const LLUtils::Exception::EventArgs event{.errorCode = LLUtils::Exception::ErrorCode::RuntimeError};
        REQUIRE(renderer.RegisterCallbacks(callbacks) == ResultCode::RC_Success);
        LLUtils::Exception::OnException.Raise(event);
        REQUIRE(notifications == 1);

        renderer.Shutdown();
        renderer.Shutdown();
        REQUIRE(renderer.GetRenderer() == nullptr);
        // Reinstalling the callback proves shutdown removed the subscription, rather than just clearing it.
        REQUIRE(renderer.RegisterCallbacks(callbacks) == ResultCode::RC_Success);
        LLUtils::Exception::OnException.Raise(event);
        REQUIRE(notifications == 1);

        REQUIRE(renderer.SetParent(reinterpret_cast<std::size_t>(*handle), nativeDisplay) == 0);
        REQUIRE(renderer.Init() == 0);
        LLUtils::Exception::OnException.Raise(event);
        REQUIRE(notifications == 2);
        renderer.Shutdown();
    }
}
