#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "UI/MessageManager.h"
#include "NullRenderer.h"
#include <RendererFactory.h>
#if defined(_WIN32)
    #include "../../OIVLib/Renderers/OIVD3D11Renderer/Source/D3D11/D3D11Error.h"
#endif
#include <RendererCore/Renderer.h>
#include <OIVAppCore/ViewerImage.h>
#include <ImageProcessing/FileImage.h>
#include <ImageProcessing/TextRasterizer.h>
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

TEST_CASE("Renderer construction publishes only successful owners", "[renderer][lifetime]")
{
    std::unique_ptr<OIV::Renderer> renderer;
    REQUIRE_THROWS_WITH(
        renderer = OIV::CreateRenderer({}, {}, {.renderer = static_cast<OIV::RendererType>(-1)}).renderer,
        Catch::Matchers::ContainsSubstring("not available"));
    REQUIRE(renderer == nullptr);
    auto created = OIV::CreateRenderer({}, {}, {.renderer = OIV::RendererType::Null});
    REQUIRE(created.type == OIV::RendererType::Null);
    REQUIRE(created.renderer != nullptr);
    renderer = std::move(created.renderer);
    REQUIRE(renderer->GetSelectedAdapter().name.empty());
    auto image = renderer->CreateImage({});
}

TEST_CASE("Separate renderers own independent lifetimes", "[renderer][lifetime]")
{
    auto first       = OIV::CreateRenderer({}, {}, {.renderer = OIV::RendererType::Null}).renderer;
    auto second      = OIV::CreateRenderer({}, {}, {.renderer = OIV::RendererType::Null}).renderer;
    auto surviving   = std::make_optional(first->CreateImage({}));
    auto independent = second->CreateImage({});
    first->Shutdown();
    first->Shutdown();
    REQUIRE_NOTHROW(first->Redraw());
    REQUIRE_NOTHROW(second->Redraw());
    surviving.reset();
    first = OIV::CreateRenderer({}, {}, {.renderer = OIV::RendererType::Null}).renderer;
    REQUIRE(first->GetSelectedAdapter().name.empty());
}

TEST_CASE("File decoding finishes without an OIV instance before renderer initialization",
          "[renderer][lifetime][loading]")
{
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
    const auto* pixels = decoded->GetBufferAt(0, 0);

    std::unique_ptr<OIV::Renderer> renderer;
    SECTION("Successful initialization wraps the existing pixels on the calling thread")
    {
        REQUIRE_NOTHROW(renderer = OIV::CreateRenderer({}, {}, {.renderer = OIV::RendererType::Null}).renderer);
        OIV::ViewerImage image{std::move(decoded), std::move(metaData), OIV::ImageSource::File, fixture.native()};
        auto presentation = renderer->CreateImage(image.pixels);
        CHECK(image.path == fixture.native());
        CHECK(image.source == OIV::ImageSource::File);
        CHECK(image.pixels->GetBufferAt(0, 0) == pixels);
    }
    SECTION("Initialization failure leaves data safe to release without a renderer")
    {
        REQUIRE_THROWS(
            renderer = OIV::CreateRenderer({}, {}, {.renderer = static_cast<OIV::RendererType>(-1)}).renderer);
        decoded.reset();
        metaData.reset();
        SUCCEED("Decoded data has no renderer registration to release");
    }
}

TEST_CASE("Decoded file data preserves EXIF orientation and failed-load behavior", "[renderer][lifetime][loading]")
{
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

    IMCodec::ItemMetaDataSharedPtr repeatedMetadata;
    auto repeated = OIV::DecodeFileImage(loader, oriented.native(), repeatedMetadata,
                                         IMCodec::PluginTraverseMode::NoTraverse);
    REQUIRE(repeated);
    CHECK(repeated->GetWidth() == decoded->GetWidth());
    CHECK(repeatedMetadata->exifData.orientation == metaData->exifData.orientation);

    const auto unsupported = temporary.path / "unsupported.txt";
    {
        std::ofstream output(unsupported);
        output << "not an image";
    }
    // Reusing the output must not retain metadata from the previous successful decode.
    CHECK(OIV::DecodeFileImage(loader, unsupported.native(), metaData, IMCodec::PluginTraverseMode::NoTraverse) ==
          nullptr);
    CHECK(metaData == nullptr);
    CHECK(OIV::DecodeFileImage(loader, unsupported.native(), metaData, IMCodec::PluginTraverseMode::NoTraverse) ==
          nullptr);
}

TEST_CASE("Rendered text becomes clean and reuses its bitmap until content changes", "[renderer][text]")
{
    FreeType::FreeTypeConnector connector;
    OIV::TextRasterizer text(&connector);
    const auto font = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() /
                      "Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf";
    text.SetFontPath(font.native());
    text.SetFontSize(24);
    text.SetDPI(96, 96);
    text.SetText(LLUTILS_TEXT("OIV"));
    CHECK(text.IsBitmapDirty());
    const auto metrics = text.GetMetrics();
    CHECK(text.IsBitmapDirty());  // Measuring still leaves the bitmap pending.
    text.GetBitmap();
    REQUIRE(text.GetBitmap() != nullptr);
    CHECK_FALSE(text.IsBitmapDirty());
    CHECK(metrics.pixelSize.x == text.GetBitmap()->GetWidth());
    CHECK(metrics.pixelSize.y == text.GetBitmap()->GetHeight());
    const auto bitmap = text.GetBitmap();

    text.SetFontSize(24);
    text.SetDPI(96, 96);
    text.SetText(LLUTILS_TEXT("OIV"));
    CHECK_FALSE(text.IsBitmapDirty());
    CHECK(text.GetBitmap() == bitmap);

    text.SetFontSize(14);
    CHECK(text.IsBitmapDirty());
    text.GetMetrics();
    CHECK(text.IsBitmapDirty());
    text.GetBitmap();
    CHECK_FALSE(text.IsBitmapDirty());
    CHECK(text.GetBitmap() != bitmap);
}

TEST_CASE("Renderer lifecycle leaves worker exception reporting independent", "[renderer][lifetime]")
{
    unsigned notifications = 0;
    auto observer = LLUtils::Exception::OnException.Subscribe([&](LLUtils::Exception::EventArgs) { ++notifications; });
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
    // Independent worker reporting must not depend on a rendering owner.
    constexpr unsigned lifecycleCount = 1000;
    for (unsigned iteration = 0; iteration < lifecycleCount; ++iteration)
    {
        auto renderer = OIV::CreateRenderer({}, {}, {.renderer = OIV::RendererType::Null}).renderer;
        renderer->Shutdown();
    }
    worker.request_stop();
    worker.join();
    CHECK(notifications > 0);
}

// Run explicitly with a native window backend and an available renderer.
TEST_CASE("Renderer shutdown releases native state before surviving presentations", "[.][renderer][native-lifecycle]")
{
    LWS::PlatformContext platform;
#ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
#else
    REQUIRE(platform.Init({.backend = LWS::BackendId::Wayland}) == LWS::Result::Success);
#endif
    LWS::Window window(platform);
    REQUIRE(window.Create() == LWS::Result::Success);
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
    auto renderer = OIV::CreateRenderer({reinterpret_cast<std::size_t>(*handle), nativeDisplay}, {}).renderer;
    auto image    = std::make_optional(renderer->CreateImage({}));
    bool released = false;
    auto listener = window.Listen(
        [&](const LWS::AnyEvent& event)
        {
            if (std::holds_alternative<LWS::EventWindowDestroying>(event))
            {
                renderer->Shutdown();
                released = true;
            }
            return LWS::EventResponse::Unhandled;
        });
    REQUIRE(listener);
    REQUIRE(window.Destroy() == LWS::Result::Success);
    REQUIRE(released);
    image.reset();
    REQUIRE_NOTHROW(renderer->Shutdown());
}

TEST_CASE("Message producers stop before queued timer work can revisit released labels",
          "[.][renderer][message-lifetime]")
{
    LWS::PlatformContext platform;
#ifdef LWS_HAS_WIN32_BACKEND
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
#else
    REQUIRE(platform.Init({.backend = LWS::BackendId::Wayland}) == LWS::Result::Success);
#endif
    LWS::Window window(platform);
    REQUIRE(window.Create() == LWS::Result::Success);
    OIV::Renderer renderer(std::make_unique<OIV::NullRenderer>());
    FreeType::FreeTypeConnector font;
    OIV::LabelManager labels(&font, std::filesystem::path("Clients/OIViewer/Resources/Fonts/CascadiaCode.ttf"));
    labels.InitializeRenderer(renderer);
    OIV::MessageManager messages(window, &labels, 5, [] {});
    messages.SetUserMessage(1, OIV::MessageFlags::Persistent, LLUTILS_TEXT("Lifetime"));
    labels.Prepare(renderer);
    // Let expiry become pending before the UI loop starts, then retire its label targets.
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    REQUIRE_NOTHROW(messages.Stop());
    REQUIRE_NOTHROW(messages.Stop());
    labels.RemoveAll();
    messages.OnWindowSizeChange({640, 480});
    REQUIRE(platform.PostTask([&platform]() { platform.RequestQuit(); }) == LWS::Result::Success);
    CHECK(platform.RunMessageLoop() == LWS::LoopResult::Quit);
    REQUIRE(window.Destroy() == LWS::Result::Success);
}

#if defined(_WIN32)
TEST_CASE("D3D11 failures preserve category and HRESULT", "[renderer][errors]")
{
    using OIV::D3D11Error;
    using OIV::RendererErrorCode;
    CHECK(D3D11Error::Classify(DXGI_ERROR_DEVICE_REMOVED) == RendererErrorCode::DeviceLost);
    CHECK(D3D11Error::Classify(E_OUTOFMEMORY) == RendererErrorCode::OutOfMemory);
    CHECK(D3D11Error::Classify(E_FAIL) == RendererErrorCode::BackendFailure);
    try
    {
        D3D11Error::HandleDeviceError(DXGI_ERROR_DEVICE_REMOVED, "Present frame");
        FAIL("Expected a renderer failure");
    }
    catch (const OIV::RendererException& failure)
    {
        CHECK(failure.GetCode() == RendererErrorCode::DeviceLost);
        CHECK(failure.GetBackend() == OIV::RendererType::D3D11);
        CHECK(failure.GetOperation() == "Present frame");
        REQUIRE(failure.GetNativeError());
        CHECK(failure.GetNativeError()->domain == OIV::RendererNativeDomain::D3D11);
        CHECK(failure.GetNativeError()->value == static_cast<std::int64_t>(DXGI_ERROR_DEVICE_REMOVED));
    }
}
#endif
