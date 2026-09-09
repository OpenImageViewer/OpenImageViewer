#include "CommandLine.h"

#include <CLI/CLI.hpp>
#include <Version.h>
#include <sstream>
#include <vector>

namespace OIV
{
    namespace
    {
        constexpr auto RendererHelpGroup = "Choose the drawing software";
        constexpr auto AdapterHelpGroup  = "Choose the graphics card";

        std::string FormatCommandLineHelp(const CLI::App* app, std::string, CLI::AppFormatMode)
        {
            CLI::Formatter formatter;
            formatter.long_option_alignment_ratio(0.2f);
            const auto renderers = GetBuiltRenderers();
            std::string choices;
            for (const auto& info : renderers)
            {
                if (!choices.empty())
                    choices += " -> ";
                choices += info.name;
            }
            const std::string_view defaultRenderer = renderers.empty() ? "none" : renderers.front().name;
            std::string help = app->get_description() + "\n\n" + app->get_usage() +
                               "\n\nOpen an image or folder:\n"
                               "  PATH                        Image or folder to open.\n"
                               "                              Leave out to start without an image.\n"
                               "                              Put paths containing spaces in quotes.\n";
            // Input tokens form one path, so the help above shows PATH rather than a list of paths.
            for (const auto& group : app->get_groups())
            {
                const auto options = app->get_options(
                    [&group](const CLI::Option* option)
                    { return option->get_group() == group && !option->get_positional(); });
                if (!options.empty())
                {
                    help += formatter.make_group(group, false, options);
                    if (group == RendererHelpGroup)
                    {
                        help += "\n  A renderer is the software used to draw images.\n"
                                "  Available renderers, in preferred order: " +
                                (choices.empty() ? "none" : choices) +
                                ".\n"
                                "  Leave this option out to let the app choose and try another if needed.\n";
                    }
                    else if (group == AdapterHelpGroup)
                    {
                        help += "\n  Leave both options out to let the app choose the graphics card.\n\n"
                                "  With --adapter-name, the app tries only matching cards. It can try\n"
                                "  another renderer too, unless you also set --renderer.\n\n"
                                "  With --adapter-index, the app uses exactly one card and one renderer.\n"
                                "  Use it with --renderer, since card numbers can differ between renderers.\n"
                                "  Without --renderer, it uses ";
                        help += defaultRenderer;
                        help += ".\n"
                                "  This option takes priority over --adapter-name.\n"
                                "  If that choice does not work, the app reports an error and exits.\n";
                        if (IsRendererAvailable(RendererType::OpenGL))
                            help += "\n  With GL, your system chooses the graphics card. These two options\n"
                                    "  cannot be used with GL.\n";
                    }
                }
            }
            help += "\nExamples:\n"
                    "  OIViewer \"photos/cat.jpg\"\n"
                    "  OIViewer \"photos\"\n";
            if (!renderers.empty())
                help += "  OIViewer --renderer " + std::string(defaultRenderer) + " \"photos/cat.jpg\"\n";
            const auto adapterRenderer = std::ranges::find(renderers, true, &RendererInfo::supportsAdapterSelection);
            if (adapterRenderer != renderers.end())
            {
                help += "  OIViewer --adapter-name NVIDIA \"photos/cat.jpg\"\n";
                help += "  OIViewer --renderer " + std::string(adapterRenderer->name) +
                        " --adapter-index 0 \"photos/cat.jpg\"\n";
            }
            return help;
        }

        void ConfigureCommandLine(CLI::App& app, CommandLineParameters& parameters,
                                  std::vector<LLUtils::native_string_type>& input)
        {
            app.option_defaults()->group("Help");
            app.set_help_flag("-h,--help", "Show this help and exit.");
            const std::string version = FormatFullVersion(CurrentVersion);
            app.set_version_flag("--version", version, "Show the application version and exit.");
            app.description("OpenImageViewer Version " + version);
            app.usage("Usage: OIViewer [OPTIONS] [PATH]");
            app.formatter_fn(FormatCommandLineHelp);
            app.add_option("input", input);
            app.add_option_function<std::string>(
                   "--renderer",
                   [&parameters](const std::string& value)
                   {
                       for (const auto& info : GetBuiltRenderers())
                           if (detail::AsciiEqual(value, info.name))
                               parameters.rendering.renderer = info.type;
                   },
                   "Use only this renderer.")
                ->check(
                    [](const std::string& value)
                    {
                        const bool built = std::ranges::any_of(GetBuiltRenderers(), [&](const auto& info)
                                                               { return detail::AsciiEqual(value, info.name); });
                        return built ? std::string{}
                                     : "Renderer " + value + " is not available; see --help for the available choices";
                    })
                ->type_name("NAME")
                ->group(RendererHelpGroup);
            app.add_option("--adapter-name", parameters.rendering.adapterName,
                           "Choose by brand or part of the card's name.\n"
                           "Examples: NVIDIA, AMD, Intel.\n"
                           "Capital letters do not matter.")
                ->check([](const std::string& value)
                        { return value.empty() ? "Provide a graphics card brand or part of its name" : ""; })
                ->type_name("NAME")
                ->group(AdapterHelpGroup);
            app.add_option("--adapter-index", parameters.rendering.adapterIndex,
                           "Choose one card by number, starting at 0.")
                ->check(CLI::NonNegativeNumber.description(""))
                ->type_name("NUMBER")
                ->group(AdapterHelpGroup);
        }
    }  // namespace

    CommandLineParseResult ParseCommandLine(int argc, const LLUtils::native_char_type* const* argv)
    {
        CLI::App app;
        CommandLineParameters parameters;
        std::vector<LLUtils::native_string_type> input;
        ConfigureCommandLine(app, parameters, input);
        try
        {
            app.parse(argc, argv);
        }
        catch (const CLI::ParseError& error)
        {
            std::ostringstream out, err;
            const int code = app.exit(error, out, err);
            return CommandLineExit{code, out.str(), err.str()};
        }
        if (!input.empty())
        {
            parameters.inputPath.emplace(std::move(input.front()));
            for (size_t i = 1; i < input.size(); ++i)
                *parameters.inputPath += LLUTILS_TEXT(" ") + input[i];
        }
        return parameters;
    }

    bool ShouldForwardInput(const CommandLineParameters& parameters)
    {
        // Explicit graphics options must reach a new instance rather than an existing process's renderer.
        return parameters.inputPath.has_value() && !parameters.inputPath->empty() && !parameters.rendering.renderer &&
               !parameters.rendering.adapterName && !parameters.rendering.adapterIndex;
    }
}  // namespace OIV
