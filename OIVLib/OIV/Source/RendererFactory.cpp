#include <RendererFactory.h>
#include "RendererSelection.h"
#include "NullRenderer.h"
#include <Version.h>
#include <LLUtils/PlatformUtility.h>
#include <cstdlib>
#include <stdexcept>

namespace OIV
{
    namespace
    {
        std::filesystem::path RendererDataRoot()
        {
#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
            const std::filesystem::path root = LLUtils::PlatformUtility::GetAppDataFolder();
#else
            std::filesystem::path root;
            if (const char* cacheHome = std::getenv("XDG_CACHE_HOME"); cacheHome != nullptr && *cacheHome != '\0')
                root = cacheHome;
            else if (const char* userHome = std::getenv("HOME"); userHome != nullptr && *userHome != '\0')
                root = std::filesystem::path(userHome) / ".cache";
            else
                root = std::filesystem::temp_directory_path();
#endif
            return root / "OIV" / FormatFullVersion(CurrentVersion) / "Renderer";
        }
    }  // namespace

    RendererCreation CreateRenderer(NativeRenderTarget target, const RendererOptions& options)
    {
        if (const auto error = ValidateRendererOptions(options); !error.empty())
            throw std::invalid_argument(error);
        const RendererInitialization initialization{
            .container     = target.container,
            .nativeDisplay = target.nativeDisplay,
            .dataPath      = RendererDataRoot(),
        };
        BackendSelection selected;
#if OIV_ALLOW_NULL_RENDERER
        if (options.renderer == RendererType::Null)
        {
            selected.backend = std::make_unique<NullRenderer>();
            selected.backend->Init(initialization);
        }
        else
#endif
            selected = SelectRenderer(GetRendererBackends(), options, initialization);
        return {.renderer = std::make_unique<Renderer>(std::move(selected.backend)), .type = selected.type};
    }
}  // namespace OIV
