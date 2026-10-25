#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <OIVAppCore/UserStorage.h>
#include <OIVShared/AtomicFile.h>
#include <LLUtils/PlatformUtility.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <array>
#include <latch>
#include <chrono>
#include <fstream>
#include <thread>
#include <optional>
#include <LLUtils/StringUtility.h>

namespace
{
    struct StorageFixture
    {
        std::filesystem::path root = std::filesystem::temp_directory_path() /
                                     ("OIViewer-storage-" +
                                      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        OIV::UserDirectories directories{root / "Config", root / "State", root / "Cache"};
        StorageFixture()
        {
            std::filesystem::create_directories(directories.config);
            std::filesystem::create_directories(directories.state);
            std::filesystem::create_directories(directories.cache / "Generated");
            std::ofstream(directories.config / "association-prompts.json") << "preferences";
            std::ofstream(directories.state / "OIViewer.old.log") << "log history";
        }
        ~StorageFixture() { std::filesystem::remove_all(root); }
    };
    std::string Read(const std::filesystem::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        return {std::istreambuf_iterator<char>(input), {}};
    }
}  // namespace

TEST_CASE("User storage invalidates only generated caches when its full version changes", "[storage][AppCore]")
{
    StorageFixture fixture;
    const auto generated = fixture.directories.cache / "Generated";
    const auto cached    = generated / "shader.bin";
    std::ofstream(cached) << "stale";
    {
        OIV::StorageSession first(fixture.directories, "1.2.3.4");
        REQUIRE_FALSE(std::filesystem::exists(cached));
        std::filesystem::create_directories(generated);
        std::ofstream(cached) << "compiled";
    }
    std::string expectedVersion = "1.2.3.4";
    SECTION("Same full version retains the cache")
    {
        OIV::StorageSession same(fixture.directories, "1.2.3.4");
        REQUIRE(Read(cached) == "compiled");
    }
    SECTION("A revision upgrade clears it")
    {
        expectedVersion = "1.2.3.5";
        OIV::StorageSession next(fixture.directories, expectedVersion);
        REQUIRE_FALSE(std::filesystem::exists(cached));
    }
    SECTION("A downgrade clears it")
    {
        expectedVersion = "1.2.3.3";
        OIV::StorageSession previous(fixture.directories, expectedVersion);
        REQUIRE_FALSE(std::filesystem::exists(cached));
    }
    SECTION("Malformed version metadata clears it")
    {
        std::ofstream(fixture.directories.cache / "cache-state.json") << "broken";
        OIV::StorageSession recovered(fixture.directories, "1.2.3.4");
        REQUIRE_FALSE(std::filesystem::exists(cached));
    }
    REQUIRE(Read(fixture.directories.config / "association-prompts.json") == "preferences");
    REQUIRE(Read(fixture.directories.state / "OIViewer.old.log") == "log history");
    std::ifstream marker(fixture.directories.cache / "cache-state.json");
    REQUIRE(nlohmann::json::parse(marker).at("version") == expectedVersion);
}

TEST_CASE("Storage sessions allow the same version and reject an active different version", "[storage][AppCore]")
{
    StorageFixture fixture;
    {
        OIV::StorageSession first(fixture.directories, "1.2.3.4");
        OIV::StorageSession second(fixture.directories, "1.2.3.4");
        REQUIRE_THROWS_WITH(OIV::StorageSession(fixture.directories, "1.2.3.5"),
                            Catch::Matchers::ContainsSubstring("Another OIViewer version"));
    }
    REQUIRE_NOTHROW(OIV::StorageSession(fixture.directories, "1.2.3.5"));
}

TEST_CASE("Cache cleanup rejects overlaps and symbolic links without touching persistent data", "[storage][AppCore]")
{
    StorageFixture fixture;
    SECTION("Config inside Generated")
    {
        fixture.directories.config = fixture.directories.cache / "Generated/config";
        std::filesystem::create_directories(fixture.directories.config);
        std::ofstream(fixture.directories.config / "keep") << "keep";
        REQUIRE_THROWS(OIV::StorageSession(fixture.directories, "1"));
        REQUIRE(Read(fixture.directories.config / "keep") == "keep");
    }
    SECTION("A redirected Generated subtree")
    {
        std::filesystem::remove(fixture.directories.cache / "Generated");
        std::error_code error;
        std::filesystem::create_directory_symlink(fixture.directories.state, fixture.directories.cache / "Generated",
                                                  error);
        if (error)
            SKIP("Creating symlinks requires privileges on this platform");
        REQUIRE_THROWS(OIV::StorageSession(fixture.directories, "1"));
        REQUIRE(Read(fixture.directories.state / "OIViewer.old.log") == "log history");
    }
    SECTION("Relative roots")
    {
        fixture.directories.cache = "relative-cache";
        REQUIRE_THROWS(OIV::StorageSession(fixture.directories, "1"));
    }
    SECTION("Shared role roots keep their files outside Generated")
    {
        fixture.directories = {fixture.root, fixture.root, fixture.root};
        std::ofstream(fixture.root / "association-prompts.json") << "shared preferences";
        std::ofstream(fixture.root / "OIViewer.log") << "shared log";
        OIV::StorageSession session(fixture.directories, "1");
        REQUIRE(Read(fixture.root / "association-prompts.json") == "shared preferences");
        REQUIRE(Read(fixture.root / "OIViewer.log") == "shared log");
    }
}

TEST_CASE("Atomic files remain complete under concurrent writers", "[storage][Shared]")
{
    StorageFixture fixture;
    const auto path = fixture.root / "atomic";
    const std::string first(8192, 'a');
    const std::string second(8192, 'b');
    std::atomic_uint completed{};
    const auto write = [&](const std::string& content)
    {
        try
        {
            OIV::WriteFileAtomically(path, std::as_bytes(std::span(content.data(), content.size())));
            ++completed;
        }
        catch (const std::exception&)
        {
            // Concurrent cache replacements may lose a Windows rename race. Readers still need a complete file.
        }
    };
    {
        std::jthread a([&] { write(first); });
        std::jthread b([&] { write(second); });
    }
    REQUIRE(completed > 0);
    const auto content = Read(path);
    REQUIRE((content == first || content == second));
    REQUIRE(std::ranges::none_of(std::filesystem::directory_iterator(fixture.root),
                                 [](const auto& entry) { return entry.path().extension() == ".tmp"; }));
}

namespace
{
    struct EnvironmentOverride
    {
        LLUtils::native_string_type name;
        std::optional<LLUtils::native_string_type> previous;
        EnvironmentOverride(LLUtils::native_string_type variable, const LLUtils::native_string_type& value)
            : name(std::move(variable))
        {
#ifdef _WIN32
            if (const auto* old = _wgetenv(name.c_str()))
                previous = old;
            _wputenv_s(name.c_str(), value.c_str());
#else
            if (const auto* old = std::getenv(name.c_str()))
                previous = old;
            setenv(name.c_str(), value.c_str(), 1);
#endif
        }
        ~EnvironmentOverride()
        {
#ifdef _WIN32
            _wputenv_s(name.c_str(), previous ? previous->c_str() : L"");
#else
            if (previous)
                setenv(name.c_str(), previous->c_str(), 1);
            else
                unsetenv(name.c_str());
#endif
        }
    };
}  // namespace

TEST_CASE("User directories follow platform conventions without version folders", "[storage][AppCore]")
{
    StorageFixture fixture;
    EnvironmentOverride home(LLUTILS_TEXT("HOME"), (fixture.root / "home").native());
    EnvironmentOverride config(LLUTILS_TEXT("XDG_CONFIG_HOME"), (fixture.root / "custom-config").native());
    EnvironmentOverride state(LLUTILS_TEXT("XDG_STATE_HOME"), (fixture.root / "custom-state").native());
    EnvironmentOverride cache(LLUTILS_TEXT("XDG_CACHE_HOME"), (fixture.root / "custom-cache").native());
#ifdef _WIN32
    const auto expected    = std::filesystem::path(LLUtils::PlatformUtility::GetAppDataFolder()) / "OIViewer";
    const auto directories = OIV::UserDirectories::Resolve();
    REQUIRE(directories.config == expected / "Config");
    REQUIRE(directories.state == expected / "State");
    REQUIRE(directories.cache == expected / "Cache");
#else
    SECTION("Absolute overrides")
    {
        const auto directories = OIV::UserDirectories::Resolve();
        REQUIRE(directories.config == fixture.root / "custom-config/OIViewer");
        REQUIRE(directories.state == fixture.root / "custom-state/OIViewer");
        REQUIRE(directories.cache == fixture.root / "custom-cache/OIViewer");
    }
    SECTION("Empty and relative overrides use HOME defaults independently")
    {
        EnvironmentOverride empty(LLUTILS_TEXT("XDG_CONFIG_HOME"), {});
        EnvironmentOverride relative(LLUTILS_TEXT("XDG_STATE_HOME"), LLUTILS_TEXT("relative"));
        const auto directories = OIV::UserDirectories::Resolve();
        REQUIRE(directories.config == fixture.root / "home/.config/OIViewer");
        REQUIRE(directories.state == fixture.root / "home/.local/state/OIViewer");
        REQUIRE(directories.cache == fixture.root / "custom-cache/OIViewer");
    }
#endif
}

TEST_CASE("Failed cache cleanup never advances the recorded version", "[storage][AppCore]")
{
    StorageFixture fixture;
    {
        OIV::StorageSession initial(fixture.directories, "1.0.0.1");
    }
    const auto generated = fixture.directories.cache / "Generated";
    std::filesystem::create_directories(generated);
    const auto cached = generated / "shader.bin";
    std::ofstream(cached) << "compiled";
    const auto before = Read(fixture.directories.cache / "cache-state.json");
#ifdef _WIN32
    const std::unique_ptr<void, decltype(&CloseHandle)> held(CreateFileW(cached.c_str(), GENERIC_READ, FILE_SHARE_READ,
                                                                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                                                         nullptr),
                                                             &CloseHandle);
    REQUIRE(held.get() != INVALID_HANDLE_VALUE);
#else
    struct Restore
    {
        std::filesystem::path path;
        ~Restore()
        {
            std::error_code error;
            std::filesystem::permissions(path, std::filesystem::perms::owner_all, error);
        }
    } restore{generated};
    std::filesystem::permissions(generated, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
#endif
    REQUIRE_THROWS(OIV::StorageSession(fixture.directories, "1.0.0.2"));
    REQUIRE(Read(fixture.directories.cache / "cache-state.json") == before);
}

TEST_CASE("Atomic replacement failures preserve existing directory contents", "[storage][Shared]")
{
    StorageFixture fixture;
    const auto destination = fixture.root / "directory";
    std::filesystem::create_directory(destination);
    std::ofstream(destination / "keep") << "keep";
    const std::string content = "replacement";
    REQUIRE_THROWS(OIV::WriteFileAtomically(destination, std::as_bytes(std::span(content.data(), content.size()))));
    REQUIRE(Read(destination / "keep") == "keep");
}

TEST_CASE("User storage supports Unicode directory names", "[storage][AppCore]")
{
    StorageFixture fixture;
    fixture.directories.cache /= LLUTILS_TEXT("\u56fe\u50cf");
    {
        OIV::StorageSession session(fixture.directories, "1.0.0.1");
    }
    REQUIRE(std::filesystem::exists(fixture.directories.cache / "cache-state.json"));
    REQUIRE_NOTHROW(OIV::StorageSession(fixture.directories, "1.0.0.1"));
}

TEST_CASE("WLX resolves its own profile without changing the standalone root", "[storage][wlx]")
{
    const auto normal = OIV::UserDirectories::Resolve();
    const auto plugin = OIV::UserDirectories::Resolve("OIViewer.WLX");
#ifdef _WIN32
    const auto roaming = std::filesystem::path(LLUtils::PlatformUtility::GetAppDataFolder());
    CHECK(normal.config == roaming / "OIViewer/Config");
    CHECK(plugin.config == roaming / "OIViewer.WLX/Config");
    CHECK(plugin.state == roaming / "OIViewer.WLX/State");
    CHECK(plugin.cache == roaming / "OIViewer.WLX/Cache");
#else
    CHECK(plugin.config == normal.config.parent_path() / "OIViewer.WLX");
    CHECK(plugin.state == normal.state.parent_path() / "OIViewer.WLX");
    CHECK(plugin.cache == normal.cache.parent_path() / "OIViewer.WLX");
#endif
    CHECK(plugin.config != normal.config);
    CHECK(plugin.state != normal.state);
    CHECK(plugin.cache != normal.cache);
}

TEST_CASE("User settings are seeded once from bundled defaults", "[storage][wlx]")
{
    StorageFixture fixture;
    fixture.directories.config /= "new-profile";
    const auto defaults        = fixture.root / "defaults.json";
    const std::string original = "{\r\n  \"fit\": true\r\n}\r\n";
    std::ofstream(defaults, std::ios::binary) << original;
    const auto settings = fixture.directories.InitializeSettings(defaults);
    CHECK(settings == fixture.directories.config / "Settings.json");
    CHECK(Read(settings) == original);
    SECTION("Changed defaults never overwrite user edits")
    {
        std::ofstream(settings) << "{\"fit\": false}";
        std::ofstream(defaults) << "{\"fit\": true, \"newSetting\": 1}";
        CHECK(fixture.directories.InitializeSettings(defaults) == settings);
        CHECK(Read(settings) == "{\"fit\": false}");
    }
    SECTION("Existing settings do not require the bundled file")
    {
        std::filesystem::remove(defaults);
        CHECK(fixture.directories.InitializeSettings(defaults) == settings);
        CHECK(Read(settings) == original);
    }
}

TEST_CASE("Missing defaults do not leave an empty user settings file", "[storage][wlx]")
{
    StorageFixture fixture;
    CHECK_THROWS(fixture.directories.InitializeSettings(fixture.root / "missing.json"));
    CHECK_FALSE(std::filesystem::exists(fixture.directories.config / "Settings.json"));
}

TEST_CASE("Concurrent first startups publish one complete settings file", "[storage][wlx]")
{
    StorageFixture fixture;
    const auto defaults       = fixture.root / "defaults.json";
    const std::string content = "{\"fit\": true, \"label\": \"shared schema\"}";
    std::ofstream(defaults, std::ios::binary) << content;
    std::latch start(1);
    std::array<std::exception_ptr, 2> errors{};
    std::array<std::filesystem::path, 2> settings;
    const auto initialize = [&](size_t index)
    {
        start.wait();
        try
        {
            settings[index] = fixture.directories.InitializeSettings(defaults);
        }
        catch (...)
        {
            errors[index] = std::current_exception();
        }
    };
    std::jthread first(initialize, 0), second(initialize, 1);
    start.count_down();
    first.join();
    second.join();
    for (const auto& error : errors)
        if (error)
            std::rethrow_exception(error);
    REQUIRE(settings[0] == settings[1]);
    CHECK(Read(settings[0]) == content);
}

TEST_CASE("Standalone and WLX cache leases do not block different versions", "[storage][wlx]")
{
    StorageFixture fixture;
    const auto pluginRoot = fixture.root / "OIViewer.WLX";
    const OIV::UserDirectories plugin{pluginRoot / "Config", pluginRoot / "State", pluginRoot / "Cache"};
    OIV::StorageSession standalone(fixture.directories, "1.0.0.1");
    OIV::StorageSession hosted(plugin, "2.0.0.1");
    CHECK(nlohmann::json::parse(Read(fixture.directories.cache / "cache-state.json"))["version"] == "1.0.0.1");
    CHECK(nlohmann::json::parse(Read(plugin.cache / "cache-state.json"))["version"] == "2.0.0.1");
    CHECK_THROWS(OIV::StorageSession(plugin, "3.0.0.1"));
    CHECK_THROWS(OIV::StorageSession(fixture.directories, "2.0.0.1"));
}

TEST_CASE("Settings initialization rejects a directory without changing its contents", "[storage][wlx]")
{
    StorageFixture fixture;
    const auto settings = fixture.directories.config / "Settings.json";
    std::filesystem::create_directory(settings);
    std::ofstream(settings / "keep") << "keep";
    CHECK_THROWS(fixture.directories.InitializeSettings(fixture.root / "unused-defaults.json"));
    CHECK(Read(settings / "keep") == "keep");
}
