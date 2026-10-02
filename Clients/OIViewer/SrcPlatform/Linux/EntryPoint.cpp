#include "Main.h"
#include "ExceptionHandler.h"
#include <LLUtils/Exception.h>
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[])
{
    if (const auto result = OIV::RunExceptionDialog(argc, argv))
        return *result;
    LLUtils::Exception::RegisterMainThread();
    const OIV::ExceptionRegistration exceptionRegistration;
    try
    {
        auto parsed       = OIV::ParseCommandLine(argc, argv);
        const auto result = std::holds_alternative<OIV::CommandLineExit>(parsed)
                                ? std::get<OIV::CommandLineExit>(std::move(parsed))
                                : RunViewer(std::get<OIV::CommandLineParameters>(parsed));
        std::cout << result.standardOutput;
        std::cerr << result.standardError;
        return result.exitCode;
    }
    catch (...)
    {
        OIV::ReportUnhandledException(std::current_exception());
        return EXIT_FAILURE;
    }
}
