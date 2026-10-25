#include "ViewerResources.h"
#include <LLUtils/PlatformUtility.h>
namespace OIV
{
    ViewerResources ResolveViewerResources(const UserDirectories& directories)
    {
        // Resolve relative to the module that contains the viewer, not the host executable. Standalone and
        // plugin frontends own storage initialization, so replacing a surface never touches profile files.
#ifdef _WIN32
        const std::filesystem::path module = LLUtils::PlatformUtility::GetDllPath();
#else
        const std::filesystem::path module = LLUtils::PlatformUtility::GetExePath();
#endif
        const auto resources = module.parent_path() / "Resources";
        return {resources, directories.InitializeSettings(resources / "Configuration" / "Settings.json"), module};
    }
}  // namespace OIV
