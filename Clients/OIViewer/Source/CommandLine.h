#pragma once

#include <Interfaces/RendererOptions.h>
#include <LLUtils/StringDefs.h>
#include <variant>

namespace OIV
{
    struct CommandLineParameters
    {
        std::optional<LLUtils::native_string_type> inputPath;
        RendererOptions rendering;
    };

    // Output text is UTF-8; platform entry points handle console encoding.
    struct CommandLineExit
    {
        int exitCode = 0;
        std::string standardOutput;
        std::string standardError;
    };

    using CommandLineParseResult = std::variant<CommandLineParameters, CommandLineExit>;
    // Native argv is UTF-16 on Windows and UTF-8 on Linux; positional tokens form one path.
    // Help, version, and parse errors return text and exit status without initializing graphics.
    CommandLineParseResult ParseCommandLine(int argc, const LLUtils::native_char_type* const* argv);
    bool ShouldForwardInput(const CommandLineParameters& parameters);
}  // namespace OIV
