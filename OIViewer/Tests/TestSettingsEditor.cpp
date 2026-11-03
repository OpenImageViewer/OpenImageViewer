#ifdef OIV_TEST_HAS_SETTINGS_EDITOR
#include <catch2/catch_test_macros.hpp>
#include "SettingsController.h"
#include "WindowPlacement.h"
#include <LWSSettings/SettingsStorage.hpp>
#include <chrono>
#include <fstream>
#ifdef _WIN32
#include <LWS/Win32/Platform.hpp>
#include <LWS/Win32/WindowExtensions.hpp>
#include <Windows.h>
#endif

namespace
{
    using Json = nlohmann::json;
#ifdef _WIN32
    HWND SettingsWindow()
    {
        HWND window{};
        EnumWindows([](HWND candidate, LPARAM output) -> BOOL
        {
            DWORD process{};
            GetWindowThreadProcessId(candidate, &process);
            wchar_t title[128]{};
            GetWindowTextW(candidate, title, 128);
            if (process == GetCurrentProcessId() && std::wstring_view(title) == L"OIViewer settings")
            {
                *reinterpret_cast<HWND*>(output) = candidate;
                return FALSE;
            }
            return TRUE;
        }, reinterpret_cast<LPARAM>(&window));
        return window;
    }
#endif
    struct Fixture
    {
        std::filesystem::path root = std::filesystem::temp_directory_path() /
            ("oiv-settings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::path path = root / "Settings.json";
        Fixture()
        {
            std::filesystem::create_directories(root);
            std::filesystem::copy_file(std::filesystem::path(OIV_TEST_CONFIGURATION_DIR) / "Settings.json", path);
        }
        ~Fixture() { std::filesystem::remove_all(root); }
        Json Read() const { std::ifstream input(path); return Json::parse(input); }
        void Write(const Json& json) const { std::ofstream output(path, std::ios::binary); output << json.dump(2) << "\r\n"; }
    };
}

TEST_CASE("Settings editor loads every bundled setting with matching defaults", "[oiviewer][settings]")
{
    Fixture fixture;
    LWSSettings::SettingsDocument document;
    REQUIRE(LWSSettings::SettingsStorage::LoadTemplateFile(document,
        std::filesystem::path(OIV_TEST_CONFIGURATION_DIR) / "SettingsTemplate.json"));
    OIV::SettingsProfile profile;
    REQUIRE(profile.Load(document, fixture.path));
    const Json defaults = fixture.Read();
    const Json values = LWSSettings::SettingsStorage::EncodeValues(document.WorkingValues()).at("values");
    size_t leaves = 0;
    auto check = [&](auto&& self, const Json& object, const std::string& prefix) -> void
    {
        for (const auto& [key, value] : object.items())
        {
            const auto id = prefix.empty() ? key : prefix + "/" + key;
            if (value.is_object()) self(self, value, id);
            else
            {
                ++leaves;
                REQUIRE(values.contains(id));
                CHECK(values.at(id) == value);
            }
        }
    };
    check(check, defaults, "");
    CHECK(values.size() == leaves);
}

TEST_CASE("Settings profile preserves unrelated fields and detects conflicting edits", "[oiviewer][settings]")
{
    Fixture fixture;
    auto original = fixture.Read();
    original["custom"] = {{"keep", 42}};
    fixture.Write(original);
    LWSSettings::SettingsDocument document;
    REQUIRE(LWSSettings::SettingsStorage::LoadTemplateFile(document,
        std::filesystem::path(OIV_TEST_CONFIGURATION_DIR) / "SettingsTemplate.json"));
    OIV::SettingsProfile profile;
    REQUIRE(profile.Load(document, fixture.path));
    REQUIRE(document.SetValue("viewsettings/quickbrowsedelay", int64_t{150}));
    auto external = fixture.Read();
    external["custom"]["keep"] = 77;
    fixture.Write(external);
    REQUIRE(profile.Save(fixture.path, document.WorkingValues()));
    CHECK(fixture.Read()["custom"]["keep"] == 77);
    CHECK(fixture.Read()["viewsettings"]["quickbrowsedelay"] == 150);
    REQUIRE(document.SetValue("viewsettings/quickbrowsedelay", int64_t{200}));
    external = fixture.Read();
    external["viewsettings"]["quickbrowsedelay"] = 180;
    fixture.Write(external);
    CHECK_FALSE(profile.Save(fixture.path, document.WorkingValues()));
    CHECK(fixture.Read()["viewsettings"]["quickbrowsedelay"] == 180);
}

TEST_CASE("Settings editor accepts supported small values from existing profiles", "[oiviewer][settings]")
{
    Fixture fixture;
    auto existing = fixture.Read();
    existing["viewsettings"]["maxzoom"] = 0.0005;
    existing["viewsettings"]["minimagesize"] = 0.0;
    existing["viewsettings"]["slideshowinterval"] = 0;
    existing["viewsettings"]["large_image"]["screenspacetexelthresholdpixels"] = 0.0005;
    existing["viewsettings"]["large_image"]["screenspaceworkfactorlog2"] = 1023.5;
    existing["animation"]["slowspeedpercent"] = 1e-9;
    fixture.Write(existing);
    LWSSettings::SettingsDocument document;
    REQUIRE(LWSSettings::SettingsStorage::LoadTemplateFile(document,
        std::filesystem::path(OIV_TEST_CONFIGURATION_DIR) / "SettingsTemplate.json"));
    OIV::SettingsProfile profile;
    REQUIRE(profile.Load(document, fixture.path));
    CHECK(document.WorkingValue("viewsettings/maxzoom") == LWSSettings::Value{0.0005});
    CHECK(document.WorkingValue("viewsettings/minimagesize") == LWSSettings::Value{0.0});
    CHECK(document.WorkingValue("viewsettings/slideshowinterval") == LWSSettings::Value{int64_t{0}});
    CHECK(document.WorkingValue("viewsettings/large_image/screenspacetexelthresholdpixels") == LWSSettings::Value{0.0005});
    CHECK(document.WorkingValue("viewsettings/large_image/screenspaceworkfactorlog2") == LWSSettings::Value{1023.5});
    CHECK(document.WorkingValue("animation/slowspeedpercent") == LWSSettings::Value{1e-9});
    CHECK_FALSE(document.SetValue("viewsettings/large_image/screenspacetexelthresholdpixels", 0.0));
    CHECK_FALSE(document.SetValue("viewsettings/maxzoom", 0.0));
    CHECK_FALSE(document.SetValue("animation/slowspeedpercent", 0.0));

}

TEST_CASE("Settings profile rejects invalid values without replacing the document or file", "[oiviewer][settings]")
{
    Fixture fixture;
    LWSSettings::SettingsDocument document;
    REQUIRE(LWSSettings::SettingsStorage::LoadTemplateFile(document,
        std::filesystem::path(OIV_TEST_CONFIGURATION_DIR) / "SettingsTemplate.json"));
    const auto before = document.WorkingValues();
    auto invalid = fixture.Read();
    invalid["viewsettings"]["quickbrowsedelay"] = 1.5;
    fixture.Write(invalid);
    OIV::SettingsProfile profile;
    CHECK_FALSE(profile.Load(document, fixture.path));
    CHECK(document.WorkingValues() == before);
    CHECK(fixture.Read() == invalid);
}
#ifdef _WIN32
TEST_CASE("Settings controller opens closes and reopens one window", "[oiviewer][settings][windows]")
{
    Fixture fixture;
    const auto configuration = fixture.root / "Configuration";
    std::filesystem::create_directories(configuration);
    std::filesystem::copy_file(std::filesystem::path(OIV_TEST_CONFIGURATION_DIR) / "SettingsTemplate.json",
                               configuration / "SettingsTemplate.json");
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    {
        OIV::SettingsController controller(platform, fixture.root, fixture.path, [](const auto&, const auto&) {});
        REQUIRE(controller.Show());
        const HWND window = SettingsWindow();
        REQUIRE(window != nullptr);
        REQUIRE(IsWindowVisible(window));
        RECT client{};
        REQUIRE(GetClientRect(window, &client));
        const auto getDpi = reinterpret_cast<UINT(WINAPI*)(HWND)>(
            GetProcAddress(GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));
        if (getDpi)
        {
            const int dpi = static_cast<int>(getDpi(window));
            CHECK(std::abs((client.right - client.left) - MulDiv(640, dpi, 96)) <= 2);
            CHECK(std::abs((client.bottom - client.top) - MulDiv(520, dpi, 96)) <= 2);
        }
        REQUIRE(controller.Show());
        HWND other = CreateWindowExW(0, L"STATIC", L"Settings activation test", WS_OVERLAPPEDWINDOW,
                                     20, 20, 200, 100, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        REQUIRE(other != nullptr);
        ShowWindow(other, SW_SHOW);
        if (SetForegroundWindow(other))
        {
            REQUIRE(controller.Show());
            CHECK(GetForegroundWindow() == window);
        }
        DestroyWindow(other);
        ShowWindow(window, SW_MINIMIZE);
        REQUIRE(IsIconic(window));
        REQUIRE(controller.Show());
        CHECK_FALSE(IsIconic(window));
        SendMessageW(window, WM_KEYDOWN, VK_ESCAPE, 1 | (1L << 30));
        CHECK(IsWindowVisible(window));
        SendMessageW(window, WM_KEYDOWN, VK_ESCAPE, 1);
        CHECK_FALSE(IsWindowVisible(window));
        REQUIRE(controller.Show());
        CHECK(IsWindowVisible(window));
        SendMessageW(window, WM_CLOSE, 0, 0);
        CHECK_FALSE(IsWindowVisible(window));
    }
    REQUIRE(platform.Shutdown() == LWS::Result::Success);
}

TEST_CASE("Closing settings restores the current file after an external change", "[oiviewer][settings][windows]")
{
    Fixture fixture;
    const auto configuration = fixture.root / "Configuration";
    std::filesystem::create_directories(configuration);
    const Json setting{{"type", "radio"}, {"id", "filesystem/deletedfileremovalmode"},
                       {"label", "Deletion"}, {"default", "none"},
                       {"options", {{{"label", "Never"}, {"value", "none"}},
                                    {{"label", "Internal"}, {"value", "internally"}},
                                    {{"label", "External"}, {"value", "externally"}}}}};
    {
        std::ofstream output(configuration / "SettingsTemplate.json", std::ios::binary);
        output << Json{{"version", 1}, {"children", Json::array({setting})}}.dump(2) << "\r\n";
    }
    auto values = fixture.Read();
    values["filesystem"]["deletedfileremovalmode"] = "none";
    fixture.Write(values);
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    {
        std::string applied = "none";
        OIV::SettingsController controller(platform, fixture.root, fixture.path,
            [&](const auto& id, const auto& value)
            {
                REQUIRE(id == "filesystem/deletedfileremovalmode");
                applied = std::get<std::string>(value);
            });
        REQUIRE(controller.Show());
        const HWND window = SettingsWindow();
        REQUIRE(window != nullptr);
        // Reach the radio field through ordinary focus traversal and commit one step.
        for (unsigned attempt = 0; attempt < 20 && applied == "none"; ++attempt)
        {
            SendMessageW(window, WM_KEYDOWN, VK_TAB, 1);
            SendMessageW(window, WM_KEYDOWN, VK_RIGHT, 1);
        }
        REQUIRE(applied == "internally");
        values["filesystem"]["deletedfileremovalmode"] = "externally";
        fixture.Write(values);
        applied = "externally";
        controller.ApplyPending([&](const auto&, const auto& value) { applied = std::get<std::string>(value); });
        REQUIRE(applied == "internally");
        SECTION("Close button") { SendMessageW(window, WM_CLOSE, 0, 0); }
        SECTION("Escape") { SendMessageW(window, WM_KEYDOWN, VK_ESCAPE, 1); }
        CHECK_FALSE(IsWindowVisible(window));
        CHECK(applied == "externally");
        CHECK(fixture.Read()["filesystem"]["deletedfileremovalmode"] == "externally");
        unsigned pending = 0;
        controller.ApplyPending([&](const auto&, const auto&) { ++pending; });
        CHECK(pending == 0);
        REQUIRE(controller.Show());
        CHECK(applied == "externally");
        SendMessageW(window, WM_CLOSE, 0, 0);
    }
    REQUIRE(platform.Shutdown() == LWS::Result::Success);
}

TEST_CASE("Settings window opens near the invoking viewer", "[oiviewer][settings][windows]")
{
    Fixture fixture;
    const auto configuration = fixture.root / "Configuration";
    std::filesystem::create_directories(configuration);
    std::filesystem::copy_file(std::filesystem::path(OIV_TEST_CONFIGURATION_DIR) / "SettingsTemplate.json",
                               configuration / "SettingsTemplate.json");
    REQUIRE(LWS::Win32::BootstrapProcess() == LWS::Result::Success);
    LWS::PlatformContext platform;
    REQUIRE(platform.Init({.backend = LWS::BackendId::Win32}) == LWS::Result::Success);
    {
        LWS::Window owner(platform);
        REQUIRE(owner.Create({.title = L"Placement owner", .clientSize = {200, 160}, .visible = true}) ==
                LWS::Result::Success);
        REQUIRE(owner.Center(LWS::CenterTarget::PrimaryMonitor) == LWS::Result::Success);
        CHECK(OIV::PlaceWindowNear(owner, owner) == LWS::Result::InvalidArgument);
        OIV::SettingsController controller(platform, fixture.root, fixture.path, [](const auto&, const auto&) {});
        REQUIRE(controller.Show(&owner));
        const HWND settings = SettingsWindow();
        REQUIRE(settings != nullptr);
        const HWND mainWindow = *LWS::Win32::GetHwnd(owner);
        RECT mainRect{}, settingsRect{};
        MONITORINFO monitor{sizeof(MONITORINFO)};
        REQUIRE(GetWindowRect(mainWindow, &mainRect));
        REQUIRE(GetWindowRect(settings, &settingsRect));
        REQUIRE(GetMonitorInfoW(MonitorFromWindow(mainWindow, MONITOR_DEFAULTTONEAREST), &monitor));
        const int workWidth = monitor.rcWork.right - monitor.rcWork.left;
        const int workHeight = monitor.rcWork.bottom - monitor.rcWork.top;
        const int settingsWidth = settingsRect.right - settingsRect.left;
        const int settingsHeight = settingsRect.bottom - settingsRect.top;
        MONITORINFO settingsMonitor{sizeof(MONITORINFO)};
        REQUIRE(GetMonitorInfoW(MonitorFromWindow(settings, MONITOR_DEFAULTTONEAREST), &settingsMonitor));
        const auto& work = settingsMonitor.rcWork;
        if (settingsWidth <= work.right - work.left && settingsHeight <= work.bottom - work.top)
        {
            CHECK(settingsRect.left >= work.left);
            CHECK(settingsRect.right <= work.right);
            CHECK(settingsRect.top >= work.top);
            CHECK(settingsRect.bottom <= work.bottom);
        }
        if (workWidth >= settingsWidth + mainRect.right - mainRect.left + 32 ||
            workHeight >= settingsHeight + mainRect.bottom - mainRect.top + 32)
        {
            INFO("owner " << mainRect.left << "," << mainRect.top << " to " << mainRect.right << "," << mainRect.bottom
                 << " settings " << settingsRect.left << "," << settingsRect.top << " to " << settingsRect.right
                 << "," << settingsRect.bottom << " work " << monitor.rcWork.left << "," << monitor.rcWork.top
                 << " to " << monitor.rcWork.right << "," << monitor.rcWork.bottom);
            const bool covered = settingsRect.left <= mainRect.left && settingsRect.top <= mainRect.top &&
                                 settingsRect.right >= mainRect.right && settingsRect.bottom >= mainRect.bottom;
            CHECK_FALSE(covered);
        }
        if (workWidth >= settingsWidth + mainRect.right - mainRect.left + 64 && settingsHeight <= workHeight)
        {
            REQUIRE(SetWindowPos(mainWindow, nullptr, monitor.rcWork.left + 20, monitor.rcWork.top + 20, 0, 0,
                                 SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
            REQUIRE(controller.Show(&owner));
            RECT movedMain{}, movedSettings{};
            REQUIRE(GetWindowRect(mainWindow, &movedMain));
            REQUIRE(GetWindowRect(settings, &movedSettings));
            CHECK((movedSettings.left != settingsRect.left || movedSettings.top != settingsRect.top));
            CHECK(movedSettings.left >= movedMain.right);
        }
    }
    REQUIRE(platform.Shutdown() == LWS::Result::Success);
}
#endif
#endif
