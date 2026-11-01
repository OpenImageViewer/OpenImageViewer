#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <array>
#include <cwchar>
#include "SrcPlatform/Win32/FileAssociations.h"
#include "SrcPlatform/Win32/FileAssociationDefaults.h"
#include <ShlObj.h>
#include <Shlwapi.h>
#include <sddl.h>
#include <bit>
#include <LLUtils/PlatformUtility.h>
#include <OIVAppCore/FileAssociationSession.h>
#include <OIVAppCore/FileAssociationPreferences.h>

#include <atomic>
#include <fstream>

namespace
{
    std::atomic_uint securityWritesUntilFailure{};
    LSTATUS WINAPI SetKeySecurityForTest(HKEY key, SECURITY_INFORMATION information, PSECURITY_DESCRIPTOR descriptor)
    {
        if (securityWritesUntilFailure != 0 && --securityWritesUntilFailure == 0)
            return ERROR_ACCESS_DENIED;
        static const auto native = reinterpret_cast<decltype(&RegSetKeySecurity)>(
            GetProcAddress(GetModuleHandleW(L"advapi32.dll"), "RegSetKeySecurity"));
        return native(key, information, descriptor);
    }
}  // namespace
// Redirect only this test executable's import. Fixture setup and cleanup still call the real API.
extern "C"
{
    decltype(&RegSetKeySecurity) __imp_RegSetKeySecurity = SetKeySecurityForTest;
}

namespace
{
    // Adapt the real native backend to the common session while keeping registry overrides fixture-local.
    std::unique_ptr<OIV::FileAssociationSession> StartAssociations(
        std::filesystem::path configDirectory, std::set<std::wstring> extensions, std::wstring executable,
        OIV::FileAssociationNotify notify, OIV::FileAssociationAction action = OIV::FileAssociationAction::Check)
    {
        return std::make_unique<OIV::FileAssociationSession>(
            std::move(configDirectory), executable,
            [extensions = std::move(extensions), executable](auto operation, auto stop, const auto& callback)
            { OIV::Win32::ManageFileAssociations(extensions, executable, callback, stop, operation); },
            std::move(notify), action);
    }

    // All writes stay beneath a unique fixture key. Real file associations and prompt preferences are untouched.
    struct UserHiveFixture
    {
        std::wstring path = L"Software\\OIViewerAssociationTests\\" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                            std::to_wstring(GetTickCount64());
        const std::wstring executableLocation = OIV::FileAssociationPolicy::ExecutableLocation(
            LLUtils::PlatformUtility::GetExePath());
        std::filesystem::path configDirectory = std::filesystem::temp_directory_path() /
                                                (L"OIViewer preferences " + std::to_wstring(GetCurrentProcessId()) +
                                                 L"-" + std::to_wstring(GetTickCount64()));
        HKEY root{};
        std::filesystem::path PreferencesFile() const { return configDirectory / "association-prompts.json"; }
        UserHiveFixture()
        {
            REQUIRE(RegCreateKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, nullptr, REG_OPTION_NON_VOLATILE,
                                    KEY_ALL_ACCESS, nullptr, &root, nullptr) == ERROR_SUCCESS);
        }
        static void DeleteChildren(HKEY parent)
        {
            std::array<wchar_t, 256> name{};
            DWORD size = static_cast<DWORD>(name.size());
            while (RegEnumKeyExW(parent, 0, name.data(), &size, nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS)
            {
                HKEY child{};
                if (RegOpenKeyExW(parent, name.data(), 0, KEY_ENUMERATE_SUB_KEYS, &child) != ERROR_SUCCESS)
                    break;
                DeleteChildren(child);
                RegCloseKey(child);
                // RegDeleteTree requests SetValue access, which deliberately protected fixture keys deny.
                if (RegDeleteKeyW(parent, name.data()) != ERROR_SUCCESS)
                    break;
                size = static_cast<DWORD>(name.size());
            }
        }

        ~UserHiveFixture()
        {
            DeleteChildren(root);
            RegCloseKey(root);
            RegDeleteKeyW(HKEY_CURRENT_USER, path.c_str());
            std::filesystem::remove_all(configDirectory);
        }

        std::wstring Read(const wchar_t* key, const wchar_t* name = nullptr) const
        {
            DWORD size{};
            REQUIRE(RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, nullptr, &size) == ERROR_SUCCESS);
            std::wstring value(size / sizeof(wchar_t), L'\0');
            REQUIRE(RegGetValueW(root, key, name, RRF_RT_REG_SZ, nullptr, value.data(), &size) == ERROR_SUCCESS);
            value.resize(size / sizeof(wchar_t) - 1);
            return value;
        }
    };
}  // namespace

TEST_CASE("Association registration advertises every extension without replacing existing defaults",
          "[associations][win32]")
{
    UserHiveFixture hive;
    const std::wstring executable = L"C:\\Image Apps\\OIViewer.exe";
    const std::set<std::wstring> extensions{L"jpg", L"jpeg", L"png"};
    const wchar_t other[]      = L"AnotherApp.Image";
    const wchar_t userChoice[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.png\\UserChoice";
    REQUIRE(RegSetKeyValueW(hive.root, L"Software\\Classes\\.png", nullptr, REG_SZ, other, sizeof(other)) ==
            ERROR_SUCCESS);
    REQUIRE(RegSetKeyValueW(hive.root, L"Software\\Classes\\.png\\OpenWithProgids", other, REG_SZ, L"",
                            sizeof(wchar_t)) == ERROR_SUCCESS);
    REQUIRE(RegSetKeyValueW(hive.root, userChoice, L"ProgId", REG_SZ, other, sizeof(other)) == ERROR_SUCCESS);

    // The second call proves repeatability, including an executable moved to another directory.
    for (const auto& path : {executable, std::wstring(L"D:\\Moved Viewer\\OIViewer.exe")})
    {
        REQUIRE(OIV::Win32::RegisterFileAssociations(hive.root, path, extensions, {}) == ERROR_SUCCESS);
        REQUIRE(hive.Read(L"Software\\Classes\\OIViewer.Image\\shell\\open\\command") == L"\"" + path + L"\" \"%1\"");
        REQUIRE(hive.Read(L"Software\\Classes\\Applications\\OIViewer.exe\\shell\\open\\command") ==
                L"\"" + path + L"\" \"%1\"");
        REQUIRE(hive.Read(L"Software\\Classes\\OIViewer.Image\\DefaultIcon") == L"\"" + path + L"\",0");
        REQUIRE(hive.Read(L"Software\\RegisteredApplications", L"OIViewer") == L"Software\\OIViewer\\Capabilities");
        for (const auto& extension : extensions)
        {
            const auto suffix = L"." + extension;
            REQUIRE(hive.Read(L"Software\\OIViewer\\Capabilities\\FileAssociations", suffix.c_str()) ==
                    L"OIViewer.Image");
            REQUIRE(
                hive.Read((L"Software\\Classes\\" + suffix + L"\\OpenWithProgids").c_str(), L"OIViewer.Image").empty());
            REQUIRE(
                hive.Read(L"Software\\Classes\\Applications\\OIViewer.exe\\SupportedTypes", suffix.c_str()).empty());
        }
        REQUIRE(hive.Read(L"Software\\Classes\\.png") == other);
        REQUIRE(hive.Read(userChoice, L"ProgId") == other);
        REQUIRE(hive.Read(L"Software\\Classes\\.png\\OpenWithProgids", other).empty());
    }
}

TEST_CASE("Association prompt preferences use the supplied config directory", "[associations][win32]")
{
    UserHiveFixture hive;
    bool shown = true;
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, hive.executableLocation));
    REQUIRE_FALSE(shown);
    REQUIRE_NOTHROW(OIV::SaveAssociationPromptChoice(hive.configDirectory, hive.executableLocation));
    REQUIRE(std::filesystem::exists(hive.PreferencesFile()));
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, hive.executableLocation));
    REQUIRE(shown);

    const auto otherLocation = OIV::FileAssociationPolicy::ExecutableLocation(
        (std::filesystem::path(hive.executableLocation).parent_path() / L"other-viewer.exe").native());
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, otherLocation));
    REQUIRE_FALSE(shown);
    REQUIRE_NOTHROW(OIV::SaveAssociationPromptChoice(hive.configDirectory, hive.executableLocation,
                                                     OIV::FileAssociationChoice::SuppressForUser));
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, otherLocation));
    REQUIRE(shown);
    REQUIRE(std::filesystem::remove(hive.PreferencesFile()));
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, hive.executableLocation));
    REQUIRE_FALSE(shown);
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, otherLocation));
    REQUIRE_FALSE(shown);
}

TEST_CASE("Prompt files preserve Unicode locations and Windows path case semantics", "[associations][win32]")
{
    UserHiveFixture hive;
    const std::wstring location      = L"C:\\images\\\u56fe\u50cf\\OIViewer.exe";
    const std::wstring alternateCase = L"c:\\images\\\u56fe\u50cf\\OIVIEWER.EXE";
    REQUIRE_NOTHROW(OIV::SaveAssociationPromptChoice(hive.configDirectory, location));
    bool shown{};
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, alternateCase));
    REQUIRE(shown);
}

TEST_CASE("A failed prompt preference write leaves the previous file intact", "[associations][win32]")
{
    UserHiveFixture hive;
    REQUIRE_NOTHROW(OIV::SaveAssociationPromptChoice(hive.configDirectory, hive.executableLocation));
    const std::unique_ptr<void, decltype(&CloseHandle)> held(CreateFileW(hive.PreferencesFile().c_str(), GENERIC_READ,
                                                                         FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                                                                         FILE_ATTRIBUTE_NORMAL, nullptr),
                                                             &CloseHandle);
    REQUIRE(held.get() != INVALID_HANDLE_VALUE);
    REQUIRE_THROWS(OIV::SaveAssociationPromptChoice(hive.configDirectory, hive.executableLocation,
                                                    OIV::FileAssociationChoice::SuppressForUser));
    bool shown{};
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, hive.executableLocation));
    REQUIRE(shown);
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, L"other-copy"));
    REQUIRE_FALSE(shown);
}

TEST_CASE("Malformed prompt files are reported without losing preferences", "[associations][win32]")
{
    UserHiveFixture hive;
    std::filesystem::create_directories(hive.PreferencesFile().parent_path());
    constexpr std::string_view invalid = "{broken preferences}\r\n";
    std::ofstream(hive.PreferencesFile(), std::ios::binary) << invalid;
    bool shown{};
    REQUIRE_THROWS(OIV::IsAssociationPromptSuppressed(hive.configDirectory, hive.executableLocation));
    REQUIRE_THROWS(OIV::SaveAssociationPromptChoice(hive.configDirectory, hive.executableLocation));
    std::ifstream input(hive.PreferencesFile(), std::ios::binary);
    REQUIRE(std::string(std::istreambuf_iterator<char>(input), {}) == invalid);
}

TEST_CASE("Cancelled registration makes no changes and registry access failures remain errors", "[associations][win32]")
{
    UserHiveFixture hive;
    std::stop_source stop;
    stop.request_stop();
    REQUIRE(OIV::Win32::RegisterFileAssociations(hive.root, L"C:\\OIViewer.exe", {L"png"}, stop.get_token()) ==
            ERROR_CANCELLED);
    HKEY key{};
    REQUIRE(RegOpenKeyExW(hive.root, L"Software", 0, KEY_READ, &key) == ERROR_FILE_NOT_FOUND);
    REQUIRE(OIV::Win32::RegisterFileAssociations(nullptr, L"C:/OIViewer.exe", {L"png"}, {}) != ERROR_SUCCESS);
}

TEST_CASE("An empty association catalog finishes silently and worker destruction cancels promptly",
          "[associations][win32]")
{
    UserHiveFixture hive;
    std::atomic_int notifications{};
    {
        auto check = StartAssociations(hive.configDirectory, {}, LLUtils::PlatformUtility::GetExePath(),
                                       [&](const auto&) { ++notifications; });
    }
    REQUIRE(notifications == 0);
    const auto start = std::chrono::steady_clock::now();
    {
        auto check = StartAssociations(hive.configDirectory, {L"oiv-test-unregistered-extension"},
                                       LLUtils::PlatformUtility::GetExePath(), [](const auto&) {});
    }
    REQUIRE(std::chrono::steady_clock::now() - start < std::chrono::seconds(5));
}

namespace
{
    // These overrides are process-local and belong to this dedicated executable. Even a Yes response can only
    // register into the fixture. Restore predefined handles after all workers and COM queries have finished.
    struct RedirectUserHive
    {
        HKEY classes{};
        explicit RedirectUserHive(const UserHiveFixture& hive)
        {
            REQUIRE(RegCreateKeyExW(hive.root, L"Software\\Classes", 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &classes,
                                    nullptr) == ERROR_SUCCESS);
            REQUIRE(RegOverridePredefKey(HKEY_CURRENT_USER, hive.root) == ERROR_SUCCESS);
            REQUIRE(RegOverridePredefKey(HKEY_CLASSES_ROOT, classes) == ERROR_SUCCESS);
        }
        ~RedirectUserHive()
        {
            RegOverridePredefKey(HKEY_CLASSES_ROOT, nullptr);
            RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
            RegCloseKey(classes);
        }
    };

    struct Notices
    {
        std::mutex mutex;
        std::condition_variable ready;
        std::vector<OIV::FileAssociationNotice> values;
        bool finished{};

        OIV::FileAssociationNotify Callback()
        {
            // The callback's last owner is the worker entry point; its destruction signals actual worker exit.
            auto completion = std::shared_ptr<void>(nullptr,
                                                    [this](void*)
                                                    {
                                                        const std::scoped_lock lock(mutex);
                                                        finished = true;
                                                        ready.notify_all();
                                                    });
            return [this, completion = std::move(completion)](OIV::FileAssociationNotice notice)
            {
                const std::scoped_lock lock(mutex);
                values.push_back(std::move(notice));
                ready.notify_all();
            };
        }

        std::vector<OIV::FileAssociationNotice> Snapshot()
        {
            const std::scoped_lock lock(mutex);
            return values;
        }

        bool Wait(bool forCompletion)
        {
            std::unique_lock lock(mutex);
            return ready.wait_for(lock, std::chrono::seconds(10),
                                  [&] { return forCompletion ? finished : !values.empty(); });
        }
    };
}  // namespace

TEST_CASE("Association checks honor all responses using isolated user registry state", "[associations][win32]")
{
    UserHiveFixture hive;
    RedirectUserHive redirect(hive);
    const auto executable = LLUtils::PlatformUtility::GetExePath();
    const std::set<std::wstring> extensions{L"oi-association-workflow-test"};
    using Kind   = OIV::FileAssociationNotice::Kind;
    using Choice = OIV::FileAssociationChoice;
    Notices first;
    auto check = StartAssociations(hive.configDirectory, extensions, executable, first.Callback());
    REQUIRE(first.Wait(false));
    REQUIRE(first.Snapshot().front().kind == Kind::Prompt);

    SECTION("Ask next time leaves preferences and associations unchanged")
    {
        check->Respond(Choice::AskNextTime);
        REQUIRE(first.Wait(true));
        REQUIRE_FALSE(std::filesystem::exists(hive.PreferencesFile()));
        Notices next;
        auto again = StartAssociations(hive.configDirectory, extensions, executable, next.Callback());
        REQUIRE(next.Wait(false));
        REQUIRE(next.Snapshot().front().kind == Kind::Prompt);
        HKEY key{};
        REQUIRE(RegOpenKeyExW(hive.root, L"Software\\OIViewer\\Capabilities", 0, KEY_READ, &key) ==
                ERROR_FILE_NOT_FOUND);
    }
    SECTION("SuppressForCopy suppresses this location while another executable can still prompt")
    {
        check->Respond(Choice::SuppressForCopy);
        REQUIRE(first.Wait(true));
        bool shown{};
        REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, hive.executableLocation));
        REQUIRE(shown);
        Notices next;
        auto again = StartAssociations(hive.configDirectory, extensions, executable, next.Callback());
        REQUIRE(next.Wait(true));
        REQUIRE(next.Snapshot().empty());
        Notices other;
        auto another = StartAssociations(
            hive.configDirectory, extensions,
            (std::filesystem::path(executable).parent_path() / L"other-viewer.exe").native(), other.Callback());
        REQUIRE(other.Wait(false));
        REQUIRE(other.Snapshot().front().kind == Kind::Prompt);
        HKEY key{};
        REQUIRE(RegOpenKeyExW(hive.root, L"Software\\OIViewer\\Capabilities", 0, KEY_READ, &key) ==
                ERROR_FILE_NOT_FOUND);
    }
    SECTION("Global opt-out suppresses all executable locations without changing defaults")
    {
        check->Respond(Choice::SuppressForUser);
        REQUIRE(first.Wait(true));
        Notices other;
        auto another = StartAssociations(
            hive.configDirectory, extensions,
            (std::filesystem::path(executable).parent_path() / L"other-viewer.exe").native(), other.Callback());
        REQUIRE(other.Wait(true));
        REQUIRE(other.Snapshot().empty());
        HKEY key{};
        REQUIRE(RegOpenKeyExW(hive.root, L"Software\\OIViewer\\Capabilities", 0, KEY_READ, &key) ==
                ERROR_FILE_NOT_FOUND);
    }
    SECTION("Associate sets defaults directly without opening Settings")
    {
        check->Respond(Choice::Associate);
        REQUIRE(first.Wait(true));
        REQUIRE(first.Snapshot().size() == 2);
        REQUIRE(first.Snapshot().back().kind == Kind::Completed);
        REQUIRE(hive.Read(L"Software\\OIViewer\\Capabilities\\FileAssociations", L".oi-association-workflow-test") ==
                L"OIViewer.Image");
        REQUIRE(hive.Read(L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.oi-association-workflow-"
                          L"test\\UserChoice",
                          L"ProgId") == L"OIViewer.Image");
        REQUIRE(OIV::Win32::QueryFileAssociation(L"oi-association-workflow-test", executable).status ==
                OIV::FileAssociationStatus::CurrentViewer);
    }
}

TEST_CASE("Association checks suppress concurrent prompts and can stop while waiting for the UI",
          "[associations][win32]")
{
    UserHiveFixture hive;
    RedirectUserHive redirect(hive);
    const auto executable = LLUtils::PlatformUtility::GetExePath();
    const std::set<std::wstring> extensions{L"oi-association-concurrent-test"};
    Notices first;
    auto check = StartAssociations(hive.configDirectory, extensions, executable, first.Callback());
    REQUIRE(first.Wait(false));
    REQUIRE(first.Snapshot().front().kind == OIV::FileAssociationNotice::Kind::Prompt);
    Notices second;
    auto concurrent = StartAssociations(hive.configDirectory, extensions, executable, second.Callback());
    REQUIRE(second.Wait(true));
    REQUIRE(second.Snapshot().empty());

    const auto before = std::chrono::steady_clock::now();
    check.reset();
    REQUIRE(std::chrono::steady_clock::now() - before < std::chrono::seconds(5));
    REQUIRE(first.Wait(true));
    REQUIRE(first.Snapshot().size() == 1);
    // Cancellation releases the file lock without saving a permanent choice.
    Notices next;
    auto afterClose = StartAssociations(hive.configDirectory, extensions, executable, next.Callback());
    REQUIRE(next.Wait(false));
    REQUIRE(next.Snapshot().front().kind == OIV::FileAssociationNotice::Kind::Prompt);
}

TEST_CASE("The effective executable distinguishes this viewer from other handlers", "[associations][win32]")
{
    UserHiveFixture hive;
    RedirectUserHive redirect(hive);
    const auto executable = LLUtils::PlatformUtility::GetExePath();
    REQUIRE(OIV::Win32::RegisterFileAssociations(hive.root, executable, {L"oi-associated-test"}, {}) == ERROR_SUCCESS);
    constexpr wchar_t progId[] = L"OIViewer.Image";
    REQUIRE(RegSetKeyValueW(hive.root, L"Software\\Classes\\.oi-associated-test", nullptr, REG_SZ, progId,
                            sizeof(progId)) == ERROR_SUCCESS);
    auto query = OIV::Win32::QueryFileAssociation(L"oi-associated-test", executable);
    REQUIRE(query.status == OIV::FileAssociationStatus::CurrentViewer);
    query = OIV::Win32::QueryFileAssociation(L"oi-associated-test", L"C:\\missing-viewer\\OIViewer.exe");
    REQUIRE(query.status == OIV::FileAssociationStatus::OtherOrMissing);
    query = OIV::Win32::QueryFileAssociation(L"oi-never-associated-test", executable);
    REQUIRE(query.status == OIV::FileAssociationStatus::OtherOrMissing);

    Notices notices;
    auto check = StartAssociations(hive.configDirectory, {L"oi-associated-test"}, executable, notices.Callback());
    REQUIRE(notices.Wait(true));
    REQUIRE(notices.Snapshot().empty());
}

TEST_CASE("UserChoice hashes match independent vectors and round FILETIME down to a minute", "[associations][win32]")
{
    using OIV::Win32::GenerateUserChoiceHash;
    const auto first = GenerateUserChoiceHash(L".png", L"S-1-5-21-100-200-300-1001", L"OIViewer.Image",
                                              134229840000000000);
    REQUIRE(first);
    REQUIRE(*first == L"06wVafAZHvc=");
    REQUIRE(GenerateUserChoiceHash(L".PNG", L"S-1-5-21-100-200-300-1001", L"OIVIEWER.IMAGE", 134229840599999999) ==
            first);
    REQUIRE(GenerateUserChoiceHash(L".png", L"S-1-5-21-100-200-300-1001", L"OIViewer.Image", 134229840600000000) !=
            first);
    const auto second = GenerateUserChoiceHash(L".JPEG", L"S-1-5-21-1-2-3-1000", L"Applications\\Viewer.EXE",
                                               132744000123456789);
    REQUIRE(second);
    REQUIRE(*second == L"TNAhRbffFDo=");
}

TEST_CASE("Adding twice and removing restores the original choice and its protected key", "[associations][win32]")
{
    UserHiveFixture hive;
    const auto sid = OIV::Win32::GetAssociationUserSid();
    REQUIRE(sid);
    const std::wstring base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.probe";
    const auto choice       = base + L"\\UserChoice";
    const auto backup       = base + L"\\OIViewer.PreviousUserChoice";
    REQUIRE(RegSetKeyValueW(hive.root, choice.c_str(), L"ProgId", REG_SZ, L"Previous.Image",
                            sizeof(L"Previous.Image")) == ERROR_SUCCESS);
    REQUIRE(RegSetKeyValueW(hive.root, choice.c_str(), L"Hash", REG_SZ, L"previous-hash", sizeof(L"previous-hash")) ==
            ERROR_SUCCESS);
    HKEY key{};
    REQUIRE(RegOpenKeyExW(hive.root, choice.c_str(), 0, KEY_ALL_ACCESS, &key) == ERROR_SUCCESS);
    const std::wstring permissions = L"D:(D;;0x2;;;" + *sid + L")(A;;KA;;;" + *sid + L")";
    PSECURITY_DESCRIPTOR descriptor{};
    REQUIRE(ConvertStringSecurityDescriptorToSecurityDescriptorW(permissions.c_str(), SDDL_REVISION_1, &descriptor,
                                                                 nullptr));
    const auto secured = RegSetKeySecurity(key, DACL_SECURITY_INFORMATION, descriptor);
    LocalFree(descriptor);
    REQUIRE(secured == ERROR_SUCCESS);
    // Capture the completed fixture. Setting its security also changes the registry timestamp.
    FILETIME previousTime{};
    REQUIRE(RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, &previousTime) == ERROR_SUCCESS);
    RegCloseKey(key);

    REQUIRE(OIV::Win32::SetDefaultFileAssociation(hive.root, L".probe", *sid).has_value());
    REQUIRE(hive.Read(choice.c_str(), L"ProgId") == L"OIViewer.Image");
    REQUIRE(hive.Read(backup.c_str(), L"ProgId") == L"Previous.Image");
    REQUIRE(hive.Read(backup.c_str(), L"Hash") == L"previous-hash");
    REQUIRE(OIV::Win32::SetDefaultFileAssociation(hive.root, L".probe", *sid).has_value());
    REQUIRE(hive.Read(backup.c_str(), L"ProgId") == L"Previous.Image");
    REQUIRE(OIV::Win32::RemoveDefaultFileAssociation(hive.root, L".probe", L"Applications\\OIViewer.exe") ==
            ERROR_SUCCESS);
    REQUIRE(hive.Read(choice.c_str(), L"ProgId") == L"Previous.Image");
    REQUIRE(hive.Read(choice.c_str(), L"Hash") == L"previous-hash");
    REQUIRE(RegOpenKeyExW(hive.root, choice.c_str(), 0, KEY_QUERY_VALUE, &key) == ERROR_SUCCESS);
    FILETIME restoredTime{};
    const auto queried = RegQueryInfoKeyW(key, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                          nullptr, nullptr, &restoredTime);
    RegCloseKey(key);
    REQUIRE(queried == ERROR_SUCCESS);
    REQUIRE(std::bit_cast<uint64_t>(restoredTime) == std::bit_cast<uint64_t>(previousTime));
    REQUIRE(RegOpenKeyExW(hive.root, choice.c_str(), 0, KEY_SET_VALUE, &key) == ERROR_ACCESS_DENIED);
}

TEST_CASE("Removal keeps newer user defaults and a fresh association can be removed completely",
          "[associations][win32]")
{
    UserHiveFixture hive;
    const auto sid = OIV::Win32::GetAssociationUserSid();
    REQUIRE(sid);
    const std::wstring base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.probe";
    const auto choice       = base + L"\\UserChoice";
    REQUIRE(OIV::Win32::SetDefaultFileAssociation(hive.root, L".probe", *sid).has_value());
    REQUIRE(OIV::Win32::RemoveDefaultFileAssociation(hive.root, L".probe", L"Applications\\OIViewer.exe") ==
            ERROR_SUCCESS);
    HKEY key{};
    REQUIRE(RegOpenKeyExW(hive.root, choice.c_str(), 0, KEY_READ, &key) == ERROR_FILE_NOT_FOUND);
    REQUIRE(RegSetKeyValueW(hive.root, choice.c_str(), L"ProgId", REG_SZ, L"Old.Image", sizeof(L"Old.Image")) ==
            ERROR_SUCCESS);
    REQUIRE(OIV::Win32::SetDefaultFileAssociation(hive.root, L".probe", *sid).has_value());
    REQUIRE(RegSetKeyValueW(hive.root, choice.c_str(), L"ProgId", REG_SZ, L"New.Image", sizeof(L"New.Image")) ==
            ERROR_SUCCESS);
    REQUIRE(OIV::Win32::RemoveDefaultFileAssociation(hive.root, L".probe", L"Applications\\OIViewer.exe") ==
            ERROR_SUCCESS);
    REQUIRE(hive.Read(choice.c_str(), L"ProgId") == L"New.Image");
    REQUIRE(RegOpenKeyExW(hive.root, (base + L"\\OIViewer.PreviousUserChoice").c_str(), 0, KEY_READ, &key) ==
            ERROR_FILE_NOT_FOUND);
}

TEST_CASE("Manual association actions bypass prompt suppression and remove registrations from older codec catalogs",
          "[associations][win32]")
{
    UserHiveFixture hive;
    RedirectUserHive redirect(hive);
    const auto executable = LLUtils::PlatformUtility::GetExePath();
    using Action          = OIV::FileAssociationAction;
    using Kind            = OIV::FileAssociationNotice::Kind;
    REQUIRE_NOTHROW(OIV::SaveAssociationPromptChoice(hive.configDirectory, hive.executableLocation,
                                                     OIV::FileAssociationChoice::SuppressForUser));
    Notices add;
    auto adding = StartAssociations(hive.configDirectory, {L"oi-manual-test", L"oi-retired-codec"}, executable,
                                    add.Callback(), Action::Add);
    REQUIRE(add.Wait(true));
    REQUIRE(add.Snapshot().size() == 1);
    REQUIRE(add.Snapshot().front().kind == Kind::Completed);
    REQUIRE(OIV::Win32::QueryFileAssociation(L"oi-manual-test", executable).status ==
            OIV::FileAssociationStatus::CurrentViewer);
    Notices remove;
    auto removing = StartAssociations(hive.configDirectory, {L"oi-manual-test"}, executable, remove.Callback(),
                                      Action::Remove);
    REQUIRE(remove.Wait(true));
    REQUIRE(remove.Snapshot().size() == 1);
    REQUIRE(remove.Snapshot().front().kind == Kind::Completed);
    HKEY key{};
    REQUIRE(RegOpenKeyExW(hive.root, L"Software\\Classes\\OIViewer.Image", 0, KEY_READ, &key) == ERROR_FILE_NOT_FOUND);
    REQUIRE(RegOpenKeyExW(hive.root,
                          L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.oi-retired-"
                          L"codec\\UserChoice",
                          0, KEY_READ, &key) == ERROR_FILE_NOT_FOUND);
    bool shown{};
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, hive.executableLocation));
    REQUIRE(shown);
    Notices next;
    auto check = StartAssociations(hive.configDirectory, {L"oi-manual-test"}, executable, next.Callback());
    REQUIRE(next.Wait(true));
    REQUIRE(next.Snapshot().empty());
}

// Explicitly opt in on a desktop with OIViewer already registered. Only a unique, previously absent extension is
// touched, and its entire FileExts key is removed afterwards. Normal tests use only redirected registry hives.
TEST_CASE("Windows accepts a generated hash for a disposable extension", "[.][associations-live]")
{
    const auto sid = OIV::Win32::GetAssociationUserSid();
    REQUIRE(sid);
    const std::wstring extension = L".oiv-association-probe-" + std::to_wstring(GetCurrentProcessId()) + L"-" +
                                   std::to_wstring(GetTickCount64());
    const std::wstring path      = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\" + extension;
    HKEY key{};
    REQUIRE(RegOpenKeyExW(HKEY_CURRENT_USER, path.c_str(), 0, KEY_READ, &key) == ERROR_FILE_NOT_FOUND);
    struct Cleanup
    {
        std::wstring path;
        ~Cleanup()
        {
            RegDeleteTreeW(HKEY_CURRENT_USER, path.c_str());
            SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, nullptr, nullptr);
        }
    } cleanup{path};
    REQUIRE(OIV::Win32::SetDefaultFileAssociation(HKEY_CURRENT_USER, extension, *sid).has_value());
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST | SHCNF_FLUSHNOWAIT, nullptr, nullptr);
    std::array<wchar_t, 256> progId{};
    DWORD count = static_cast<DWORD>(progId.size());
    REQUIRE(AssocQueryStringW(ASSOCF_NONE, ASSOCSTR_PROGID, extension.c_str(), nullptr, progId.data(), &count) == S_OK);
    REQUIRE(std::wstring(progId.data()) == L"OIViewer.Image");
    REQUIRE(OIV::Win32::RemoveDefaultFileAssociation(HKEY_CURRENT_USER, extension, L"Applications\\OIViewer.exe") ==
            ERROR_SUCCESS);
    REQUIRE(RegOpenKeyExW(HKEY_CURRENT_USER, (path + L"\\UserChoice").c_str(), 0, KEY_READ, &key) ==
            ERROR_FILE_NOT_FOUND);
}

TEST_CASE("A failed default restoration keeps the current choice available", "[associations][win32]")
{
    UserHiveFixture hive;
    const auto sid = OIV::Win32::GetAssociationUserSid();
    REQUIRE(sid);
    const std::wstring base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.probe";
    REQUIRE(RegSetKeyValueW(hive.root, (base + L"\\UserChoice").c_str(), L"ProgId", REG_SZ, L"Previous.Image",
                            sizeof(L"Previous.Image")) == ERROR_SUCCESS);
    REQUIRE(OIV::Win32::SetDefaultFileAssociation(hive.root, L".probe", *sid).value());
    REQUIRE_FALSE(OIV::Win32::SetDefaultFileAssociation(hive.root, L".probe", *sid).value());
    HKEY backup{};
    REQUIRE(RegOpenKeyExW(hive.root, (base + L"\\OIViewer.PreviousUserChoice").c_str(), 0, KEY_ALL_ACCESS, &backup) ==
            ERROR_SUCCESS);
    const std::wstring permissions = L"D:(D;;KW;;;" + *sid + L")(A;;KA;;;" + *sid + L")";
    PSECURITY_DESCRIPTOR descriptor{};
    REQUIRE(ConvertStringSecurityDescriptorToSecurityDescriptorW(permissions.c_str(), SDDL_REVISION_1, &descriptor,
                                                                 nullptr));
    const auto secured = RegSetKeySecurity(backup, DACL_SECURITY_INFORMATION, descriptor);
    LocalFree(descriptor);
    REQUIRE(secured == ERROR_SUCCESS);
    const auto removed = OIV::Win32::RemoveDefaultFileAssociation(hive.root, L".probe", L"Applications\\OIViewer.exe");
    // Use the retained handle to release fixture protection even when the restoration assertion fails.
    SECURITY_DESCRIPTOR unprotected{};
    InitializeSecurityDescriptor(&unprotected, SECURITY_DESCRIPTOR_REVISION);
    SetSecurityDescriptorDacl(&unprotected, TRUE, nullptr, FALSE);
    RegSetKeySecurity(backup, DACL_SECURITY_INFORMATION, &unprotected);
    RegCloseKey(backup);
    REQUIRE(removed == ERROR_ACCESS_DENIED);
    REQUIRE(hive.Read((base + L"\\UserChoice").c_str(), L"ProgId") == L"OIViewer.Image");
    REQUIRE(hive.Read((base + L"\\OIViewer.PreviousUserChoice").c_str(), L"ProgId") == L"Previous.Image");
}

TEST_CASE("Shutdown before a response leaves the prompt enabled", "[associations][win32]")
{
    UserHiveFixture hive;
    RedirectUserHive redirect(hive);
    Notices notices;
    {
        auto check = StartAssociations(hive.configDirectory, {L"oi-prompt-close-test"},
                                       LLUtils::PlatformUtility::GetExePath(), notices.Callback());
        REQUIRE(notices.Wait(false));
        REQUIRE(notices.Snapshot().front().kind == OIV::FileAssociationNotice::Kind::Prompt);
        bool shown{};
        REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, hive.executableLocation));
        REQUIRE_FALSE(shown);
    }
    Notices next;
    auto check = StartAssociations(hive.configDirectory, {L"oi-prompt-close-test"},
                                   LLUtils::PlatformUtility::GetExePath(), next.Callback());
    REQUIRE(next.Wait(false));
    REQUIRE(next.Snapshot().front().kind == OIV::FileAssociationNotice::Kind::Prompt);
}

TEST_CASE("A global prompt opt-out survives immediate shutdown", "[associations][win32]")
{
    UserHiveFixture hive;
    RedirectUserHive redirect(hive);
    Notices notices;
    {
        auto check = StartAssociations(hive.configDirectory, {L"oi-global-close-test"},
                                       LLUtils::PlatformUtility::GetExePath(), notices.Callback());
        REQUIRE(notices.Wait(false));
        REQUIRE(notices.Snapshot().front().kind == OIV::FileAssociationNotice::Kind::Prompt);
        check->Respond(OIV::FileAssociationChoice::SuppressForUser);
    }
    bool shown{};
    REQUIRE_NOTHROW(shown = OIV::IsAssociationPromptSuppressed(hive.configDirectory, L"C:\\other\\OIViewer.exe"));
    REQUIRE(shown);
}

TEST_CASE("A post-rename security failure rolls the active choice back", "[associations][win32]")
{
    UserHiveFixture hive;
    const auto sid = OIV::Win32::GetAssociationUserSid();
    REQUIRE(sid);
    const std::wstring base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.rollback";
    const auto choice       = base + L"\\UserChoice";
    REQUIRE(RegSetKeyValueW(hive.root, choice.c_str(), L"ProgId", REG_SZ, L"Previous.Image",
                            sizeof(L"Previous.Image")) == ERROR_SUCCESS);
    HKEY key{};
    REQUIRE(RegOpenKeyExW(hive.root, choice.c_str(), 0, KEY_ALL_ACCESS, &key) == ERROR_SUCCESS);
    const std::wstring permissions = L"D:(D;;0x2;;;" + *sid + L")(A;;KA;;;" + *sid + L")";
    PSECURITY_DESCRIPTOR descriptor{};
    REQUIRE(ConvertStringSecurityDescriptorToSecurityDescriptorW(permissions.c_str(), SDDL_REVISION_1, &descriptor,
                                                                 nullptr));
    const auto secured = RegSetKeySecurity(key, DACL_SECURITY_INFORMATION, descriptor);
    LocalFree(descriptor);
    RegCloseKey(key);
    REQUIRE(secured == ERROR_SUCCESS);
    securityWritesUntilFailure = 2;  // Allow temporary access, then fail DACL restoration after the rename.
    const auto result          = OIV::Win32::SetDefaultFileAssociation(hive.root, L".rollback", *sid);
    securityWritesUntilFailure = 0;
    REQUIRE_FALSE(result.has_value());
    REQUIRE(result.error() == ERROR_ACCESS_DENIED);
    REQUIRE(hive.Read(choice.c_str(), L"ProgId") == L"Previous.Image");
    REQUIRE(RegOpenKeyExW(hive.root, (base + L"\\OIViewer.PendingUserChoice").c_str(), 0, KEY_READ, &key) ==
            ERROR_FILE_NOT_FOUND);
}

TEST_CASE("Removal classifies application defaults by the owning executable name", "[associations][win32]")
{
    UserHiveFixture hive;
    // A renamed running copy owns Preview.exe. OIViewer.exe can be another independently selected copy.
    const std::wstring ownedApplicationProgId = L"Applications\\Preview.exe";
    const auto [current, previous, expected]  = GENERATE(
        (std::array<const wchar_t*, 3>{L"Applications\\OIViewer.exe", L"Old.Image", L"Applications\\OIViewer.exe"}),
        (std::array<const wchar_t*, 3>{L"Applications\\pReViEw.ExE", L"Old.Image", L"Old.Image"}),
        (std::array<const wchar_t*, 3>{L"OIViewer.Image", L"Applications\\OIViewer.exe",
                                       L"Applications\\OIViewer.exe"}),
        (std::array<const wchar_t*, 3>{L"OIViewer.Image", L"Applications\\pReViEw.ExE", nullptr}));
    const std::wstring base = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.ownership";
    const auto choice       = base + L"\\UserChoice";
    const auto backup       = base + L"\\OIViewer.PreviousUserChoice";
    REQUIRE(RegSetKeyValueW(hive.root, choice.c_str(), L"ProgId", REG_SZ, current,
                            static_cast<DWORD>((std::wcslen(current) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS);
    REQUIRE(RegSetKeyValueW(hive.root, backup.c_str(), L"ProgId", REG_SZ, previous,
                            static_cast<DWORD>((std::wcslen(previous) + 1) * sizeof(wchar_t))) == ERROR_SUCCESS);
    REQUIRE(OIV::Win32::RemoveDefaultFileAssociation(hive.root, L".ownership", ownedApplicationProgId) ==
            ERROR_SUCCESS);
    HKEY key{};
    if (expected != nullptr)
        REQUIRE(hive.Read(choice.c_str(), L"ProgId") == expected);
    else
        REQUIRE(RegOpenKeyExW(hive.root, choice.c_str(), 0, KEY_READ, &key) == ERROR_FILE_NOT_FOUND);
    REQUIRE(RegOpenKeyExW(hive.root, backup.c_str(), 0, KEY_READ, &key) == ERROR_FILE_NOT_FOUND);
}

TEST_CASE("Malformed registry choices fail without changing the current default", "[associations][win32]")
{
    UserHiveFixture hive;
    const std::wstring parent     = L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\FileExts\\.invalid";
    const auto choice             = parent + L"\\UserChoice";
    constexpr DWORD invalidProgId = 42;
    REQUIRE(RegSetKeyValueW(hive.root, choice.c_str(), L"ProgId", REG_DWORD, &invalidProgId, sizeof(invalidProgId)) ==
            ERROR_SUCCESS);
    const auto sid = OIV::Win32::GetAssociationUserSid();
    REQUIRE(sid);
    REQUIRE_FALSE(OIV::Win32::SetDefaultFileAssociation(hive.root, L".invalid", *sid).has_value());
    REQUIRE(OIV::Win32::RemoveDefaultFileAssociation(hive.root, L".invalid", L"Applications\\OIViewer.exe") !=
            ERROR_SUCCESS);
    DWORD retained{};
    DWORD size = sizeof(retained);
    REQUIRE(RegGetValueW(hive.root, choice.c_str(), L"ProgId", RRF_RT_REG_DWORD, nullptr, &retained, &size) ==
            ERROR_SUCCESS);
    REQUIRE(retained == invalidProgId);
    HKEY key{};
    REQUIRE(RegOpenKeyExW(hive.root, (parent + L"\\OIViewer.PendingUserChoice").c_str(), 0, KEY_READ, &key) ==
            ERROR_FILE_NOT_FOUND);
}
