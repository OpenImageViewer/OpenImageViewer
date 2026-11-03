#include <catch2/catch_test_macros.hpp>

#ifdef _WIN32
    #include "SrcPlatform/Win32/FileWatcherWin32.h"
    #include <chrono>
    #include <filesystem>
    #include <fstream>
    #include <future>

namespace
{
    struct WatchFolder
    {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("oiv-watch-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        WatchFolder() { REQUIRE(std::filesystem::create_directory(path)); }
        ~WatchFolder()
        {
            std::error_code ignored;
            std::filesystem::remove(path / "changed.txt", ignored);
            std::filesystem::remove(path, ignored);
        }
    };

    struct StopNotifications
    {
        OIV::IFileWatcher& watcher;
        ~StopNotifications() { watcher.StopNotifications(); }
    };
}

TEST_CASE("File notifications stop before their subscription is released", "[event][filewatcher][win32]")
{
    WatchFolder fixture;
    const auto& folder = fixture.path;
    OIV::Win32::FileWatcherWin32 watcher;
    std::promise<void> first;
    auto notified = first.get_future();
    std::atomic<unsigned> calls{};
    auto subscription = watcher.GetFileChangedEvent().Subscribe(
        [&](const auto&)
        {
            if (calls.fetch_add(1) == 0)
                first.set_value();
        });
    StopNotifications stop{watcher};
    const auto id = watcher.AddFolder(folder.native());
    {
        std::ofstream file(folder / "changed.txt");
        file << "change";
    }
    const auto notification = notified.wait_for(std::chrono::seconds(5));
    watcher.StopNotifications();
    subscription.Unsubscribe();
    // Stop drains delivery without discarding the registrations needed by controller cleanup.
    CHECK(watcher.IsFolderRegistered(folder.native()));
    watcher.RemoveFolder(id);
    CHECK_FALSE(watcher.IsFolderRegistered(folder.native()));
    CHECK(notification == std::future_status::ready);
    CHECK(calls.load() > 0);
}

TEST_CASE("Removed watches retire pending requests before their storage is released", "[event][filewatcher][win32]")
{
    WatchFolder fixture;
    const auto& folder = fixture.path;
    OIV::Win32::FileWatcherWin32 watcher;
    std::atomic<OIV::IFileWatcher::FolderID> active{};
    std::atomic<bool> signaled{};
    std::promise<void> received;
    auto done = received.get_future();
    auto subscription = watcher.GetFileChangedEvent().Subscribe([&](const auto& event)
    {
        if (event.folderID == active.load() && event.fileName == LLUTILS_TEXT("changed.txt") && !signaled.exchange(true))
            received.set_value();
    });
    StopNotifications stop{watcher};
    OIV::IFileWatcher::FolderID previous{};
    for (unsigned iteration = 0; iteration < 200; ++iteration)
    {
        const auto id = watcher.AddFolder(folder.native());
        REQUIRE(id > previous);
        previous = id;
        { std::ofstream file(folder / "changed.txt", std::ios::app); file << iteration; }
        watcher.RemoveFolder(id);
        CHECK_FALSE(watcher.IsFolderRegistered(folder.native()));
    }
    const auto id = watcher.AddFolder(folder.native());
    REQUIRE(id > previous);
    active.store(id);
    { std::ofstream file(folder / "changed.txt", std::ios::app); file << "final"; }
    const auto notification = done.wait_for(std::chrono::seconds(5));
    watcher.RemoveFolder(id);
    watcher.StopNotifications();
    subscription.Unsubscribe();
    CHECK(notification == std::future_status::ready);
    for (unsigned iteration = 0; iteration < 20; ++iteration)
    {
        OIV::Win32::FileWatcherWin32 pending;
        pending.AddFolder(folder.native());
    }
}

TEST_CASE("A watcher can be removed from its notification callback", "[event][filewatcher][win32]")
{
    WatchFolder fixture;
    const auto& folder = fixture.path;
    OIV::Win32::FileWatcherWin32 watcher;
    std::promise<void> removed;
    auto done = removed.get_future();
    std::atomic<bool> called{};
    auto subscription = watcher.GetFileChangedEvent().Subscribe([&](const auto& event)
    {
        if (!called.exchange(true))
        {
            watcher.RemoveFolder(event.folderID);
            removed.set_value();
        }
    });
    StopNotifications stop{watcher};
    watcher.AddFolder(folder.native());
    { std::ofstream file(folder / "changed.txt"); file << "changed"; }
    const auto notification = done.wait_for(std::chrono::seconds(5));
    watcher.StopNotifications();
    subscription.Unsubscribe();
    REQUIRE(notification == std::future_status::ready);
    CHECK_FALSE(watcher.IsFolderRegistered(folder.native()));
}
#endif
