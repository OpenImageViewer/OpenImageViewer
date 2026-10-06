#include "Main.h"
#include "ViewerApplication.h"
#include "HandledException.h"
#include <LLUtils/Exception.h>
#include <LLUtils/Logging/Logger.h>
#include <LLUtils/Logging/LogFileSink.h>
#include <LLUtils/Logging/LogConsoleSink.h>
#include <Version.h>
#include <LWS/Platform.hpp>
#ifdef LWS_HAS_WIN32_BACKEND
    #include <LWS/Win32/Platform.hpp>
#endif

#include <cstdlib>

namespace
{
    // Construct before platform/viewer resources so logging outlives every producer and teardown callback.
    class ViewerLogging
    {
      public:

        ViewerLogging()
        {
            LLUtils::LoggerOptions options;
            const auto folder = std::filesystem::path(OIV::ViewerApplication::GetAppDataFolder()) /
                                OIV::FormatFullVersion(OIV::CurrentVersion);
            try
            {
                options.sinks.push_back(
                    {std::make_shared<LLUtils::FileLogSink>(LLUtils::LogFileOptions{.path = folder / "oiv"})});
                options.emergencyPath = folder / "oiv.emergency.log";
            }
            catch (...)
            {
                LLUtils::Logger::Emergency("Unable to open viewer log file\n");
            }
            options.sinks.push_back({std::make_shared<LLUtils::ConsoleLogSink>(), LLUtils::LogLevel::Warning});
#ifdef _DEBUG
            options.sinks.push_back({std::make_shared<LLUtils::DebugLogSink>()});
#endif
            fOwnsSession = LLUtils::Logger::Initialize(std::move(options)) == LLUtils::LogResult::Success;
            if (!fOwnsSession)
                LLUtils::Logger::Emergency("Unable to initialize viewer logging\n");
        }
        ~ViewerLogging()
        {
            if (fOwnsSession && LLUtils::Logger::Shutdown() != LLUtils::LogResult::Success)
                LLUtils::Logger::Emergency("Viewer logging shutdown reported output failure\n");
        }

      private:

        bool fOwnsSession = false;
    };
}  // namespace

OIV::CommandLineExit RunViewer(const OIV::CommandLineParameters& parameters, ForwardFileCallback forwardFile)
{
    if (const auto error = OIV::ValidateRendererOptions(parameters.rendering); !error.empty())
        return {EXIT_FAILURE, {}, "OIViewer: " + error + "\n"};
    if (forwardFile != nullptr && OIV::ShouldForwardInput(parameters) && forwardFile(*parameters.inputPath))
        return {};

    const ViewerLogging logging;
    const auto startupCategory = LLUtils::Logger::RegisterCategory("Startup");
    LL_LOG(startupCategory, LLUtils::LogLevel::Info, "OIViewer {} revision {} ({})",
           OIV::FormatFullVersion(OIV::CurrentVersion), OIV_GIT_SHORT_HASH,
#ifdef _WIN32
           "Windows"
#else
           "Linux"
#endif
    );
    LL_LOG(startupCategory, LLUtils::LogLevel::Info, "Renderer option {} adapter index {}",
           parameters.rendering.renderer ? static_cast<int>(*parameters.rendering.renderer) : -1,
           parameters.rendering.adapterIndex ? static_cast<int>(*parameters.rendering.adapterIndex) : -1);

#ifdef LWS_HAS_WIN32_BACKEND
    if (LWS::Win32::BootstrapProcess() != LWS::Result::Success)
        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to configure Win32 process state");
#endif

    LWS::PlatformContext platform;
    const LWS::PlatformConfig platformConfig{
#ifdef LWS_HAS_WIN32_BACKEND
        .backend = LWS::BackendId::Win32,
#elif defined(LWS_HAS_WAYLAND_BACKEND)
        .backend = LWS::BackendId::Wayland,
#endif
    };
    if (platform.Init(platformConfig) != LWS::Result::Success)
        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to initialize the LWS platform");

    platform.SetUnhandledExceptionHandler(OIV::ReportHandledException);
    LWS::LoopResult loopResult;
    {
        OIV::ViewerApplication viewerApplication(platform);
        viewerApplication.Init(parameters.inputPath.value_or(LLUtils::native_string_type{}), parameters.rendering);
        loopResult = viewerApplication.Run();
    }
    if (platform.Shutdown() != LWS::Result::Success)
        LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "Unable to shut down the LWS platform");
    if (loopResult == LWS::LoopResult::Failed)
    {
        // The context retains its diagnostic after application and backend cleanup.
        const auto& failure = *platform.GetFailure();
        return {EXIT_FAILURE,
                {},
                "LWS backend failure: " + failure.operation + " (native error " + std::to_string(failure.nativeError) +
                    ")\n"};
    }
    return {};
}
