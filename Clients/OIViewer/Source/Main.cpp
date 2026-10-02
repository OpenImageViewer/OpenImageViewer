#include "Main.h"
#include "ViewerApplication.h"
#include "HandledException.h"
#include <LLUtils/Exception.h>
#include <LWS/Platform.hpp>
#ifdef LWS_HAS_WIN32_BACKEND
    #include <LWS/Win32/Platform.hpp>
#endif

#include <cstdlib>

OIV::CommandLineExit RunViewer(const OIV::CommandLineParameters& parameters, ForwardFileCallback forwardFile)
{
    if (const auto error = OIV::ValidateRendererOptions(parameters.rendering); !error.empty())
        return {EXIT_FAILURE, {}, "OIViewer: " + error + "\n"};
    if (forwardFile != nullptr && OIV::ShouldForwardInput(parameters) && forwardFile(*parameters.inputPath))
        return {};

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
