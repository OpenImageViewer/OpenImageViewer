#include "Main.h"
#include <cstdlib>
#include <iostream>

int main(int argc, char* argv[])
{
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
    catch (const std::exception& error)
    {
        std::cerr << "OIViewer: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
