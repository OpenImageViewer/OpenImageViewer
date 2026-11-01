#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_message.hpp>
#include "SrcPlatform/Linux/FileAssociations.h"

#include <gio/gdesktopappinfo.h>
#include <glib/gstdio.h>
#include <filesystem>
#include <fstream>
#include <thread>

namespace
{
    struct IsolatedDesktop
    {
        std::filesystem::path root;
        IsolatedDesktop()
        {
            char* directory = g_dir_make_tmp("oiv-associations-XXXXXX", nullptr);
            REQUIRE(directory != nullptr);
            root = directory;
            g_free(directory);
            // GIO caches XDG paths. This dedicated process sets them before the first association call.
            g_setenv("XDG_DATA_HOME", (root / "data").c_str(), TRUE);
            g_setenv("XDG_CONFIG_HOME", (root / "config").c_str(), TRUE);
            std::filesystem::create_directories(root / "data/applications");
            std::filesystem::create_directories(root / "config");
        }
        ~IsolatedDesktop() { std::filesystem::remove_all(root); }
    };

    std::string DefaultApplication(const char* mimeType, const char* expected = nullptr)
    {
        // Installing a desktop entry becomes visible asynchronously to GIO default lookups.
        std::string result;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        do
        {
            GAppInfo* application = g_app_info_get_default_for_type(mimeType, FALSE);
            const char* id        = application != nullptr ? g_app_info_get_id(application) : nullptr;
            result                = id != nullptr ? id : "";
            if (application != nullptr)
                g_object_unref(application);
            if (expected != nullptr && result != expected)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (expected != nullptr && result != expected && std::chrono::steady_clock::now() < deadline);
        return result;
    }
}  // namespace

TEST_CASE("Linux associations use codec MIME types and preserve user choices", "[associations][linux]")
{
    IsolatedDesktop desktop;
    using Action              = OIV::FileAssociationAction;
    using Kind                = OIV::FileAssociationNotice::Kind;
    const auto executablePath = desktop.root / "viewer with spaces % \" $ `";
    // A wrapper keeps argv[0] as "touch" for multicall implementations such as uutils coreutils.
    std::ofstream(executablePath) << "#!/bin/sh\nexec /usr/bin/touch \"$@\"\n";
    std::filesystem::permissions(executablePath, std::filesystem::perms::owner_read |
                                                     std::filesystem::perms::owner_write |
                                                     std::filesystem::perms::owner_exec);
    const auto executable    = executablePath.string();
    const auto binaryDefault = DefaultApplication("application/octet-stream");
    const std::set<std::string> extensions{"png", "jpg", "oiv-unknown-extension"};
    const auto run = [&](Action action)
    { return OIV::Linux::ManageFileAssociations(desktop.root / "config/OIViewer", extensions, executable, action); };

    // Establish a real desktop entry in the isolated user directory as the original PNG default.
    const auto previousPath = desktop.root / "data/applications/previous.desktop";
    std::ofstream(previousPath) << "[Desktop Entry]\nType=Application\nName=Previous\nExec=/bin/true "
                                   "%f\nMimeType=image/png;\n";
    GAppInfo* previous = G_APP_INFO(g_desktop_app_info_new_from_filename(previousPath.c_str()));
    REQUIRE(previous != nullptr);
    REQUIRE(g_app_info_set_as_default_for_type(previous, "image/png", nullptr));
    REQUIRE(DefaultApplication("image/png", "previous.desktop") == "previous.desktop");

    REQUIRE(run(Action::Check).kind == Kind::Prompt);
    const auto backupPath = desktop.root / "config/OIViewer/associations.ini";
    REQUIRE_FALSE(std::filesystem::exists(backupPath.parent_path()));
    // Checking defaults must not parse, repair, or overwrite the undo journal.
    std::filesystem::create_directories(backupPath.parent_path());
    std::ofstream(backupPath) << "[broken";
    REQUIRE(run(Action::Check).kind == Kind::Prompt);
    std::ifstream invalidBackup(backupPath);
    REQUIRE(std::string(std::istreambuf_iterator<char>(invalidBackup), {}) == "[broken");
    invalidBackup.close();
    REQUIRE(run(Action::Add).kind == Kind::ActionFailed);
    std::filesystem::remove(backupPath);
    const auto added = run(Action::Add);
    INFO(added.message);
    REQUIRE(added.kind == Kind::Completed);
    REQUIRE(DefaultApplication("application/octet-stream") == binaryDefault);
    REQUIRE(DefaultApplication("image/png", "org.openimageviewer.OIViewer.desktop") ==
            "org.openimageviewer.OIViewer.desktop");
    REQUIRE(DefaultApplication("image/jpeg", "org.openimageviewer.OIViewer.desktop") ==
            "org.openimageviewer.OIViewer.desktop");
    // Launch through GIO as a file manager would. The disposable executable records the exact file argument.
    const auto openedFile = desktop.root / "opened image % \" $ `.png";
    GAppInfo* installed   = G_APP_INFO(g_desktop_app_info_new("org.openimageviewer.OIViewer.desktop"));
    REQUIRE(installed != nullptr);
    GFile* file = g_file_new_for_path(openedFile.c_str());
    GList files{.data = file};
    const bool launched = g_app_info_launch(installed, &files, nullptr, nullptr);
    g_object_unref(file);
    g_object_unref(installed);
    REQUIRE(launched);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!std::filesystem::exists(openedFile) && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    REQUIRE(std::filesystem::exists(openedFile));
    REQUIRE(OIV::Linux::ManageFileAssociations(desktop.root / "config/OIViewer", extensions, "/usr/bin/false",
                                               Action::Remove)
                .kind == Kind::ActionFailed);
    REQUIRE(DefaultApplication("image/png", "org.openimageviewer.OIViewer.desktop") ==
            "org.openimageviewer.OIViewer.desktop");
    REQUIRE(run(Action::Add).kind == Kind::Completed);
    REQUIRE(run(Action::Remove).kind == Kind::Completed);
    REQUIRE(DefaultApplication("image/png", "previous.desktop") == "previous.desktop");
    REQUIRE(run(Action::Check).kind == Kind::Prompt);
    REQUIRE_FALSE(std::filesystem::exists(desktop.root / "data/applications/org.openimageviewer.OIViewer.desktop"));

    REQUIRE(run(Action::Add).kind == Kind::Completed);
    const auto laterPath = desktop.root / "data/applications/later.desktop";
    std::ofstream(laterPath) << "[Desktop Entry]\nType=Application\nName=Later\nExec=/bin/false "
                                "%f\nMimeType=image/png;\n";
    GAppInfo* later = G_APP_INFO(g_desktop_app_info_new_from_filename(laterPath.c_str()));
    REQUIRE(later != nullptr);
    REQUIRE(g_app_info_set_as_default_for_type(later, "image/png", nullptr));
    // Removal must keep a later external choice and include registrations from older codec sets.
    REQUIRE(OIV::Linux::ManageFileAssociations(desktop.root / "config/OIViewer", {}, executable, Action::Remove).kind ==
            Kind::Completed);
    REQUIRE(DefaultApplication("image/png", "later.desktop") == "later.desktop");
    REQUIRE(run(Action::Remove).kind == Kind::Completed);
    g_object_unref(later);
    g_object_unref(previous);

    std::stop_source cancelled;
    cancelled.request_stop();
    REQUIRE(OIV::Linux::ManageFileAssociations(desktop.root / "config/OIViewer", extensions, executable, Action::Add,
                                               cancelled.get_token())
                .message.empty());
    REQUIRE_FALSE(std::filesystem::exists(desktop.root / "data/applications/org.openimageviewer.OIViewer.desktop"));
}
