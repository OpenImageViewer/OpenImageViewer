#pragma once
#include <filesystem>
#include <memory>
#include <string_view>

namespace OIV
{
    class FileLock;

    struct UserDirectories
    {
        std::filesystem::path config;
        std::filesystem::path state;
        std::filesystem::path cache;
        // The frontend supplies one trusted directory name, keeping its settings, state, and cache lease
        // independent of other profiles. The default preserves the standalone application's storage root.
        static UserDirectories Resolve(std::string_view applicationName = "OIViewer");
        // Seed a complete settings file from bundled defaults on first use. Existing user settings win,
        // and concurrent startups serialize initialization before atomically publishing the file.
        std::filesystem::path InitializeSettings(const std::filesystem::path& defaults) const;
        std::filesystem::path GeneratedCache() const { return cache / "Generated"; }
    };

    // Owns the cache lease until all renderer and logging resources have been destroyed. Startup cleanup is
    // serialized, and a different version cannot invalidate caches while another session is using them.
    class StorageSession final
    {
      public:

        StorageSession(UserDirectories directories, std::string_view version);
        ~StorageSession();
        StorageSession(const StorageSession&)            = delete;
        StorageSession& operator=(const StorageSession&) = delete;
        const UserDirectories& Directories() const { return fDirectories; }

      private:

        UserDirectories fDirectories;
        std::unique_ptr<FileLock> fCacheLease;
    };
}  // namespace OIV
