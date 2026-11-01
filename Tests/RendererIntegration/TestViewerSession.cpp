#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <OIVAppCore/ViewerSession.h>
#include <LLUtils/PlatformUtility.h>
#include <OIVShared/ViewTransformController.h>
#include <deque>
#include <memory>
#include <thread>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <OIVAppCore/MessageHelper.h>

namespace
{
    OIV::ViewerImage SessionImage()
    {
        auto item                                = std::make_shared<IMCodec::ImageItem>();
        item->descriptor.width                   = 640;
        item->descriptor.height                  = 480;
        item->descriptor.rowPitchInBytes         = 640 * 4;
        item->descriptor.texelFormatDecompressed = IMCodec::TexelFormat::I_R8_G8_B8_A8;
        item->itemType                           = IMCodec::ImageItemType::Image;
        item->data.Allocate(640 * 480 * 4);
        std::fill_n(item->data.data(), item->data.Size(), std::byte{0x80});
        return {.pixels = std::make_shared<IMCodec::Image>(item, IMCodec::ImageItemType::Unknown)};
    }
}  // namespace

TEST_CASE("Session presentation geometry has no surface or renderer dependency", "[session][appcore]")
{
    OIV::ViewerSession session;
    session.OpenImage(SessionImage());
    session.SetViewport({320, 240});
    session.Refresh();
    CHECK(session.FitScale() == Catch::Approx(0.5));
    CHECK_THROWS(session.ImageSize(static_cast<OIV::ImageSizeType>(99)));
    const auto generation = session.Image().GetSourceGeneration();
    const auto pixels     = session.Image().GetVisibleImage();
    session.SetSelection({{10, 20}, {40, 60}});
    session.SetOffset({-100, -80});
    const LLUtils::PointF64 anchor{80, 60};
    const auto imagePoint = OIV::ViewTransformController::ClientToImage(anchor, 1.0, session.Image().GetOffset());
    REQUIRE(session.ZoomTo(2.0, {80, 60}, {160, 120}));
    const auto clientPoint = OIV::ViewTransformController::ImageToClient(imagePoint, 2.0, session.Image().GetOffset());
    CHECK(clientPoint == anchor);
    CHECK(session.Image().GetSourceGeneration() == generation);
    CHECK(session.Image().GetVisibleImage() == pixels);
    session.SetViewport({800, 600});
    CHECK(session.Selection().GetCorner(LLUtils::TopLeft) == LLUtils::PointI32(10, 20));
    CHECK(session.Selection().GetCorner(LLUtils::BottomRight) == LLUtils::PointI32(40, 60));
    CHECK(session.Image().GetScale().x == 2.0);
    REQUIRE(session.ApplySetting({.type = OIV::AppSettingsPolicy::ActionType::MaxZoom, .floatValue = 3.0}));
    CHECK(session.ZoomTo(4.0, {-1, -1}, {400, 300}));
    CHECK(session.Image().GetScale().x == 3.0);
}

TEST_CASE("Detached sessions retain typed completions and invalidate queued notifications on destruction",
          "[session][lifetime]")
{
    std::deque<std::move_only_function<void()>> posted;
    auto post = [&](std::move_only_function<void()> task)
    {
        posted.push_back(std::move(task));
        return true;
    };
    auto session = std::make_unique<OIV::ViewerSession>(nullptr, nullptr, post);
    unsigned wakes{};
    session->Attach([&] { ++wakes; });
    session->QueueCompletion(OIV::CountColorsData{7, 42});
    REQUIRE(posted.size() == 1);
    session->Detach();
    auto task = std::move(posted.front());
    posted.pop_front();
    task();
    CHECK(wakes == 0);
    session->Attach([&] { ++wakes; });
    REQUIRE(posted.size() == 1);
    task = std::move(posted.front());
    posted.pop_front();
    task();
    CHECK(wakes == 1);
    const auto batch = session->TakeCompletions();
    REQUIRE(batch.size() == 1);
    CHECK(std::get<OIV::CountColorsData>(batch[0].data).colorCount == 42);
    CHECK_THROWS(session->Attach([] {}));
    session->QueueCompletion(OIV::CountColorsData{8, 43});
    REQUIRE(posted.size() == 1);
    session.reset();
    task = std::move(posted.front());
    posted.pop_front();
    CHECK_NOTHROW(task());
    CHECK(wakes == 1);
}

TEST_CASE("Session completion posting retries without losing a batch", "[session][lifetime]")
{
    std::move_only_function<void()> posted;
    bool fail = true;
    OIV::ViewerSession session(nullptr, nullptr,
                               [&](std::move_only_function<void()> task)
                               {
                                   if (fail)
                                       return false;
                                   posted = std::move(task);
                                   return true;
                               });
    unsigned wakes{};
    session.Attach([&] { ++wakes; });
    session.QueueCompletion(OIV::CountColorsData{1, 10});
    CHECK_FALSE(posted);
    fail = false;
    session.QueueCompletion(OIV::CountColorsData{1, 20});
    REQUIRE(posted);
    posted();
    CHECK(wakes == 1);
    CHECK(session.TakeCompletions().size() == 2);
}

TEST_CASE("Key binding help formats frontend configuration without locating an executable", "[session][configuration]")
{
    const OIV::ConfigurationLoader::KeyBindingList bindings{{"Ctrl+F24", "plugin-command"}};
    const OIV::ConfigurationLoader::CommandGroupList commands{
        {"plugin-command", "Plugin-specific action", "cmd_zoom", ""}};
    const auto text = OIV::MessageHelper::CreateKeyBindingsMessage(bindings, commands);
    CHECK(text.find(LLUTILS_TEXT("Ctrl+F24")) != LLUtils::native_string_type::npos);
    CHECK(text.find(LLUTILS_TEXT("Plugin-specific action")) != LLUtils::native_string_type::npos);
}

TEST_CASE("Session animation advances only at its deadline without a native presenter", "[session][animation]")
{
    auto root      = std::make_shared<IMCodec::ImageItem>();
    root->itemType = IMCodec::ImageItemType::Container;
    auto animation = std::make_shared<IMCodec::Image>(root, IMCodec::ImageItemType::AnimationFrame);
    auto first     = SessionImage().pixels;
    auto second    = SessionImage().pixels;
    first->GetImageItem()->animationData.delayMilliseconds  = 100;
    second->GetImageItem()->animationData.delayMilliseconds = 200;
    animation->SetSubImage(0, first);
    animation->SetSubImage(1, second);
    OIV::ViewerSession session;
    session.OpenImage({.pixels = animation});
    const auto start = OIV::SequencerPolicy::Clock::time_point{};
    REQUIRE(session.StartAnimation(start) == 100);
    const auto early = session.AdvanceAnimation(start + std::chrono::milliseconds(40));
    CHECK_FALSE(early.changed);
    CHECK(early.intervalMs == 60);
    CHECK(session.Image().GetSelectedSource() == first);
    const auto next = session.AdvanceAnimation(start + std::chrono::milliseconds(100));
    CHECK(next.changed);
    CHECK(next.intervalMs == 200);
    CHECK(session.Image().GetSelectedSource() == second);
    const auto wrapped = session.AdvanceAnimation(start + std::chrono::milliseconds(300));
    CHECK(wrapped.changed);
    CHECK(session.Image().GetSelectedSource() == first);
}

TEST_CASE("Session color counting rejects a stale source and completes the current one", "[session][colors]")
{
    OIV::ViewerSession session;
    session.OpenImage(SessionImage());
    session.RequestColorCount();
    session.RequestColorCount();
    REQUIRE(session.Image().GetColorCountState() == OIV::ColorCountState::Pending);
    session.OpenImage(SessionImage());
    const auto drain = [&]
    {
        std::vector<OIV::ViewerCompletion> batch;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (batch.empty() && std::chrono::steady_clock::now() < deadline)
        {
            batch = session.TakeCompletions();
            if (batch.empty())
                std::this_thread::yield();
        }
        return batch;
    };
    auto batch = drain();
    REQUIRE(batch.size() == 1);
    CHECK_FALSE(session.CompleteColorCount(std::get<OIV::CountColorsData>(batch[0].data), true));
    CHECK(session.Image().GetColorCountState() == OIV::ColorCountState::Pending);
    batch = drain();
    REQUIRE(batch.size() == 1);
    CHECK(session.CompleteColorCount(std::get<OIV::CountColorsData>(batch[0].data), true));
    CHECK(session.Image().GetNumUniqueColors() == 1);
}

#ifdef OIV_TEST_VIEWER
    #include "Viewer.h"
    #ifdef _WIN32
        #include <LWS/Win32/Platform.hpp>
        #include <LWS/Win32/WindowExtensions.hpp>
TEST_CASE("Hosted wheel input reaches the focused composite without raw input", "[session][renderer][wlx][input]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    OIV::ViewerSession session;
    session.OpenImage(SessionImage());
    session.Refresh();
    const auto source    = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path();
    const auto resources = source / "OIViewer/Resources";
    const OIV::UserDirectories directories{source / "build/session-config", source / "build/session-state",
                                           source / "build/session-cache"};
    OIV::Viewer viewer(platform, session, directories,
                       {resources, resources / "Configuration/Settings.json",
                        std::filesystem::path(LLUtils::PlatformUtility::GetExePath())},
                       {.forwardKey = [](uint32_t, uint32_t, uint32_t) {}});
    viewer.Init({}, {.renderer = OIV::RendererType::Null}, {.clientSize = {640, 480}});
    viewer.CompleteInitialization();
    const HWND window = *LWS::Win32::GetHwnd(viewer.GetWindow().GetWindow());
    POINT point{50, 50};
    REQUIRE(ClientToScreen(window, &point));
    const auto previous = session.Image().GetScale();
    SendMessageW(window, WM_MOUSEWHEEL, MAKEWPARAM(0, WHEEL_DELTA), MAKELPARAM(point.x, point.y));
    CHECK(session.Image().GetScale().x > previous.x);
}

TEST_CASE("Viewer redraws keep animation timers running", "[session][renderer][animation]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    auto root      = std::make_shared<IMCodec::ImageItem>();
    root->itemType = IMCodec::ImageItemType::Container;
    auto animation = std::make_shared<IMCodec::Image>(root, IMCodec::ImageItemType::AnimationFrame);
    for (uint16_t index = 0; index < 2; ++index)
    {
        auto frame                                             = SessionImage().pixels;
        frame->GetImageItem()->animationData.delayMilliseconds = 50;
        animation->SetSubImage(index, frame);
    }
    OIV::ViewerSession session;
    session.OpenImage({.pixels = animation});
    session.StartAnimation();
    session.Refresh();
    const auto source    = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path();
    const auto resources = source / "OIViewer/Resources";
    const OIV::UserDirectories directories{source / "build/session-config", source / "build/session-state",
                                           source / "build/session-cache"};
    OIV::Viewer viewer(platform, session, directories,
                       {resources, resources / "Configuration/Settings.json",
                        std::filesystem::path(LLUtils::PlatformUtility::GetExePath())},
                       {});
    viewer.Init({}, {.renderer = OIV::RendererType::Null}, {.clientSize = {640, 480}});
    viewer.CompleteInitialization();
    auto previous       = session.Image().GetSelectedSource();
    unsigned changes    = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (changes < 3 && std::chrono::steady_clock::now() < deadline)
    {
        REQUIRE(platform.ProcessMessages() == LWS::LoopResult::Continue);
        const auto current = session.Image().GetSelectedSource();
        if (current != previous)
        {
            ++changes;
            previous = current;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(changes >= 3);
}

TEST_CASE("Replacing a native viewer preserves session pixels and selection", "[session][renderer][wlx]")
{
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    const auto root      = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path();
    const auto resources = root / "OIViewer/Resources";
    const OIV::UserDirectories directories{root / "build/session-config", root / "build/session-state",
                                           root / "build/session-cache"};
    OIV::ViewerSession session;
    session.OpenImage(SessionImage());
    session.Refresh();
    const auto generation = session.Image().GetSourceGeneration();
    const auto pixels     = session.Image().GetSelectedSource();
    for (int iteration = 0; iteration < 2; ++iteration)
    {
        OIV::Viewer viewer(platform, session, directories,
                           {resources, resources / "Configuration/Settings.json",
                            std::filesystem::path(LLUtils::PlatformUtility::GetExePath())},
                           {});
        REQUIRE_NOTHROW(viewer.Init({}, {.renderer = OIV::RendererType::Null}, {.clientSize = {640, 480}}));
        REQUIRE_NOTHROW(viewer.CompleteInitialization());
        if (iteration == 0)
        {
            session.SetSelection({{1, 2}, {20, 30}});
            CHECK(session.ZoomTo(2.0, {-1, -1}, {160, 120}));
        }
        CHECK(session.Image().GetSourceGeneration() == generation);
        CHECK(session.Image().GetSelectedSource() == pixels);
        CHECK_NOTHROW(viewer.Stop());
        CHECK_NOTHROW(viewer.Stop());
    }
    CHECK(session.Selection().GetCorner(LLUtils::TopLeft) == LLUtils::PointI32(1, 2));
    CHECK(session.Selection().GetCorner(LLUtils::BottomRight) == LLUtils::PointI32(20, 30));
    CHECK(session.Image().GetScale().x == 2.0);
    REQUIRE(platform.Shutdown() == LWS::Result::Success);
}
TEST_CASE("Replacing a fully initialized viewer preserves pending file navigation", "[session][renderer][wlx]")
{
    struct Files
    {
        std::filesystem::path root = std::filesystem::temp_directory_path() /
                                     ("oiv-session-browse-" +
                                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        Files()
        {
            std::filesystem::create_directories(root);
            std::ofstream(root / "a.jpg").put('a');
            std::ofstream(root / "b.jpg").put('b');
        }
        ~Files() { std::filesystem::remove_all(root); }
    } files;
    class Processor final : public OIV::RequestProcessorType
    {
        bool ProcessResidencyRequest(const OIV::ImageResidencyCacheKey&, OIV::ImageResidencyCacheValue& image) override
        {
            image = SessionImage().pixels;
            return true;
        }
    };
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    const auto resources = std::filesystem::path(OIV_TEST_SOURCE_DIR).parent_path() / "OIViewer/Resources";
    const OIV::UserDirectories directories{files.root / "config", files.root / "state", files.root / "cache"};
    {
        OIV::ImageResidencyCache cache(std::make_unique<Processor>(), 1);
        OIV::ViewerSession session(nullptr, &cache, [&platform](std::move_only_function<void()> task)
                                   { return platform.PostTask(std::move(task)) == LWS::Result::Success; });
        auto image   = SessionImage();
        image.source = OIV::ImageSource::File;
        image.path   = (files.root / "a.jpg").native();
        session.OpenImage(std::move(image));
        const OIV::ViewerResources paths{resources, resources / "Configuration/Settings.json",
                                         std::filesystem::path(LLUtils::PlatformUtility::GetExePath())};
        {
            OIV::Viewer viewer(platform, session, directories, paths, {});
            viewer.Init({}, {.renderer = OIV::RendererType::Null}, {.clientSize = {640, 480}});
            viewer.CompleteInitialization();
            REQUIRE(viewer.ExecutePredefinedCommand("NextImageInFolder"));
            viewer.Stop();
        }
        {
            OIV::Viewer viewer(platform, session, directories, paths, {});
            viewer.Init({}, {.renderer = OIV::RendererType::Null}, {.clientSize = {640, 480}});
            viewer.CompleteInitialization();
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            const auto expected = (files.root / "b.jpg").native();
            while (session.Image().GetOpenedImage()->path != expected && std::chrono::steady_clock::now() < deadline)
            {
                REQUIRE(platform.ProcessMessages() == LWS::LoopResult::Continue);
                std::this_thread::yield();
            }
            CHECK(session.Image().GetOpenedImage()->path == expected);
        }
    }
    REQUIRE(platform.Shutdown() == LWS::Result::Success);
}
    #endif
#endif

TEST_CASE("Derived images retain their source save name without a file identity", "[session][appcore]")
{
    OIV::ViewerSession session;
    auto original   = SessionImage();
    original.source = OIV::ImageSource::File;
    original.path   = LLUTILS_TEXT("C:/pictures/original.photo.jpg");
    session.OpenImage(std::move(original));
    const auto name = session.Image().GetOpenedImage()->sourceName;
    REQUIRE(name == LLUTILS_TEXT("original.photo"));
    auto derived       = SessionImage();
    derived.sourceName = name;
    session.OpenImage(std::move(derived));
    REQUIRE(session.Image().GetOpenedImage()->sourceName == name);
    REQUIRE(session.Image().GetOpenedImage()->path.empty());
    REQUIRE(session.Image().GetOpenedImage()->source == OIV::ImageSource::GeneratedByLib);
}
