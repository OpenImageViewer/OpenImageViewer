#pragma once
#include <OIVAppCore/IFileWatcher.h>
#include <filesystem>
#include <memory>
#include <OIVAppCore/UserStorage.h>
namespace OIV
{
    // Resolve installation and profile paths in the frontend, never from the file manager's executable.
    struct ViewerResources
    {
        std::filesystem::path directory;
        std::filesystem::path settingsFile;
        std::filesystem::path modulePath;
    };

    std::unique_ptr<IFileWatcher> CreateViewerFileWatcher();
    ViewerResources ResolveViewerResources(const UserDirectories& directories);
}  // namespace OIV
