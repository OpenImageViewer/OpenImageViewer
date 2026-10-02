#include <catch2/catch_test_macros.hpp>

#ifdef _WIN32
    #include "SrcPlatform/Win32/FileWatcherWin32.h"
    #include <chrono>
    #include <filesystem>
    #include <fstream>
    #include <future>

TEST_CASE("File notifications stop before their subscription is released", "[event][filewatcher][win32]")
{
    const auto folder = std::filesystem::temp_directory_path() /
                        ("oiv-owned-watch-" +
                         std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    REQUIRE(std::filesystem::create_directory(folder));
    struct Cleanup
    {
        std::filesystem::path folder;
        ~Cleanup()
        {
            std::error_code ignored;
            std::filesystem::remove(folder / "changed.txt", ignored);
            std::filesystem::remove(folder, ignored);
        }
    } cleanup{folder};
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
    struct Stop
    {
        OIV::IFileWatcher& watcher;
        ~Stop() { watcher.StopNotifications(); }
    } stop{watcher};
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
#endif
