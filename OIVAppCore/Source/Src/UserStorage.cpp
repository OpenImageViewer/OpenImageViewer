#include <OIVAppCore/UserStorage.h>
#include <OIVShared/AtomicFile.h>
#include <OIVAppCore/FileLock.h>
#include <LLUtils/Exception.h>
#include <LLUtils/PlatformUtility.h>
#include <LLUtils/ScopedFileLock.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#ifdef _WIN32
    #include <Windows.h>
#else
    #include <cstdlib>
#endif

namespace OIV
{
    namespace
    {
        void ValidateCleanupBoundary(const UserDirectories& directories)
        {
            const auto generated = directories.GeneratedCache();
#ifdef _WIN32
            const DWORD attributes = GetFileAttributesW(generated.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES && GetLastError() != ERROR_FILE_NOT_FOUND &&
                GetLastError() != ERROR_PATH_NOT_FOUND)
                LL_EXCEPTION_SYSTEM_ERROR("Could not inspect the generated cache directory");
            const bool redirected = attributes != INVALID_FILE_ATTRIBUTES &&
                                    (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
#else
            const bool redirected = std::filesystem::is_symlink(std::filesystem::symlink_status(generated));
#endif
            if (redirected)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters,
                             "OIViewer's Generated cache cannot be a symlink or junction");
            const auto componentEqual = [](const std::filesystem::path& left, const std::filesystem::path& right)
            {
#ifdef _WIN32
                return CompareStringOrdinal(left.c_str(), -1, right.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
                return left == right;
#endif
            };
            // Consuming the entire Generated path means it is a component prefix of persistent storage.
            if (std::ranges::mismatch(generated, directories.config, componentEqual).in1 == generated.end() ||
                std::ranges::mismatch(generated, directories.state, componentEqual).in1 == generated.end())
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters,
                             "OIViewer cache cleanup overlaps persistent user data");
        }
    }  // namespace

    UserDirectories UserDirectories::Resolve(std::string_view applicationName)
    {
#ifdef _WIN32
        const auto roaming = LLUtils::PlatformUtility::GetAppDataFolder();
        if (roaming.empty())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Windows roaming AppData is unavailable");
        const auto root = std::filesystem::path(roaming) / applicationName;
        return {root / "Config", root / "State", root / "Cache"};
#else
        const auto resolve = [applicationName](const char* variable, const char* suffix)
        {
            std::filesystem::path base;
            if (const char* configured = std::getenv(variable); configured != nullptr)
                base = configured;
            if (!base.is_absolute())
            {
                const char* home = std::getenv("HOME");
                if (home == nullptr || !std::filesystem::path(home).is_absolute())
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState,
                                 "An absolute HOME is required for OIViewer user directories");
                base = std::filesystem::path(home) / suffix;
            }
            return base / applicationName;
        };
        return {resolve("XDG_CONFIG_HOME", ".config"), resolve("XDG_STATE_HOME", ".local/state"),
                resolve("XDG_CACHE_HOME", ".cache")};
#endif
    }

    std::filesystem::path UserDirectories::InitializeSettings(const std::filesystem::path& defaults) const
    {
        const auto settings    = config / "Settings.json";
        const auto hasSettings = [&]
        {
            const auto status = std::filesystem::status(settings);
            if (std::filesystem::exists(status) && !std::filesystem::is_regular_file(status))
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "User settings must be a regular file");
            return std::filesystem::exists(status);
        };
        if (!hasSettings())
        {
            std::filesystem::create_directories(config);
            const LLUtils::ScopedFileLock initialization(config / ".settings-init.lock");
            if (!hasSettings())
            {
                std::ifstream input(defaults, std::ios::binary);
                if (!input)
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Could not open bundled settings");
                const std::string content{std::istreambuf_iterator<char>(input), {}};
                if (input.bad())
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Could not read bundled settings");
                WriteFileAtomically(settings, std::as_bytes(std::span(content.data(), content.size())));
            }
        }
        return settings;
    }

    StorageSession::StorageSession(UserDirectories directories, std::string_view version)
        : fDirectories(std::move(directories))
    {
        if (version.empty())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "OIViewer storage requires a version");
        if (!fDirectories.config.is_absolute() || !fDirectories.state.is_absolute() ||
            !fDirectories.cache.is_absolute())
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::BadParameters, "OIViewer user directories must be absolute");
        fDirectories.config = std::filesystem::weakly_canonical(fDirectories.config);
        fDirectories.state  = std::filesystem::weakly_canonical(fDirectories.state);
        fDirectories.cache  = std::filesystem::weakly_canonical(fDirectories.cache);
        ValidateCleanupBoundary(fDirectories);
        std::filesystem::create_directories(fDirectories.cache);
        const LLUtils::ScopedFileLock startup(fDirectories.cache / ".startup.lock");
        fCacheLease       = std::make_unique<FileLock>(fDirectories.cache / ".active.lock");
        const auto marker = fDirectories.cache / "cache-state.json";
        std::string previousVersion;
        if (std::filesystem::exists(marker))
        {
            std::ifstream input(marker);
            if (!input)
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError,
                             "Could not read OIViewer cache version state");
            try
            {
                previousVersion = nlohmann::json::parse(input).value("version", std::string{});
            }
            catch (const nlohmann::json::exception&)
            { /* Invalid metadata makes only generated caches disposable. */
            }
            if (input.bad())
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError,
                             "Could not read OIViewer cache version state");
        }
        if (previousVersion != version)
        {
            if (!fCacheLease->Lock(true, false))
                LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState,
                             "Another OIViewer version is running. Close it before starting this version.");
            // Revalidate while holding the gate, before the only recursive delete in the storage lifecycle.
            ValidateCleanupBoundary(fDirectories);
            std::filesystem::remove_all(fDirectories.GeneratedCache());
            const auto content = nlohmann::json{{"version", version}}.dump() + "\r\n";
            WriteFileAtomically(marker, std::as_bytes(std::span(content.data(), content.size())));
            fCacheLease->Unlock();
        }
        // The gate prevents another startup from entering between exclusive cleanup and the shared lease.
        fCacheLease->Lock(false, true);
    }
    StorageSession::~StorageSession() = default;
}  // namespace OIV
