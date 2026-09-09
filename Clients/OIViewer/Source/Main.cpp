#include "Main.h"
#include "ViewerApplication.h"
#include "ExceptionHandler.h"
#include <LLUtils/Exception.h>
#include <LWS/Platform.hpp>
#ifdef LWS_HAS_WIN32_BACKEND
    #include <LWS/Win32/Platform.hpp>
#endif
#include <cstdlib>
#include <stdexcept>

OIV::CommandLineExit RunViewer(const OIV::CommandLineParameters& parameters, ForwardFileCallback forwardFile)
{
    if (const auto error = OIV::ValidateRendererOptions(parameters.rendering); !error.empty())
        return {EXIT_FAILURE, {}, "OIViewer: " + error + "\n"};
    if (forwardFile != nullptr && OIV::ShouldForwardInput(parameters) && forwardFile(*parameters.inputPath))
        return {};

    struct ExceptionRegistration
    {
        ExceptionRegistration() { OIV::RegisterExceptionhandler(); }
        ~ExceptionRegistration() { OIV::RemoveExceptionHandler(); }
    } exceptionRegistration;
    try
    {
#ifdef LWS_HAS_WIN32_BACKEND
        if (LWS::Win32::BootstrapProcess() != LWS::Result::Success)
            throw std::runtime_error("Unable to configure Win32 process state");
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
            throw std::runtime_error("Unable to initialize the LWS platform");

        platform.SetUnhandledExceptionHandler(
            [](std::exception_ptr exception) noexcept
            {
                try
                {
                    std::rethrow_exception(exception);
                }
                catch (const std::exception& error)
                {
                    LL_EXCEPTION_DONT_THROW(LLUtils::Exception::ErrorCode::RuntimeError, error.what());
                }
                catch (...)
                {
                    LL_EXCEPTION_DONT_THROW(LLUtils::Exception::ErrorCode::Unknown, "Unhandled UI callback exception");
                }
            });
        {
            OIV::ViewerApplication viewerApplication(platform);
            viewerApplication.Init(parameters.inputPath.value_or(LLUtils::native_string_type{}), parameters.rendering);
            viewerApplication.Run();
        }
        if (platform.Shutdown() != LWS::Result::Success)
            throw std::runtime_error("Unable to shut down the LWS platform");
        return {};
    }
    catch (const std::exception& exception)
    {
        return {EXIT_FAILURE, {}, std::string("OIViewer: ") + exception.what() + "\n"};
    }
    catch (...)
    {
        return {EXIT_FAILURE, {}, "OIViewer: Unhandled application exception\n"};
    }
}
