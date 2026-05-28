#include <LLUtils/StringDefs.h>
#include "ImageMagickTestCorpus.h"

#include <catch2/catch_all.hpp>

#include <LLUtils/StringUtility.h>
#include <OIVShared/FileSorter.h>

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <map>
#include <optional>
#include <regex>
#include <sstream>
#include <string_view>
#include <tuple>

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif

namespace OIV::Tests
{
    namespace
    {
        constexpr std::string_view GeneratorSchemaVersion = "oiv-image-compatibility-generator-v1";

        using json       = nlohmann::json;
        using ValueMap   = std::map<std::string, std::string>;
        using OutputMap  = std::map<std::string, std::vector<std::filesystem::path>>;
        using ContextSet = std::vector<ValueMap>;

        struct CommandResult
        {
            int exitCode = 0;
            std::string output;
        };

        struct CorpusConfig
        {
            json root;
            std::string rawContent;
        };

        struct MagickEnvironment
        {
            std::filesystem::path executablePath;
            std::string versionOutput;
            std::string formatOutput;
        };

        struct MagickVersion
        {
            int major = 0;
            int minor = 0;
            int patch = 0;
            int build = 0;
        };

        struct RequiredMagick
        {
            std::string display;
            MagickVersion version;
            std::vector<std::string> formats;
        };

        std::filesystem::path GetCorpusConfigPath()
        {
#ifdef OIV_TEST_SOURCE_DIR
            return std::filesystem::path(OIV_TEST_SOURCE_DIR) / "Source" / "ImageMagickCorpus.json";
#else
            return std::filesystem::current_path() / "Source" / "ImageMagickCorpus.json";
#endif
        }

        std::string QuoteArgument(const std::string& value)
        {
            std::string quoted = "\"";
            for (const char ch : value)
            {
                if (ch == '"')
                    quoted += "\\\"";
                else
                    quoted += ch;
            }
            quoted += "\"";
            return quoted;
        }

        std::filesystem::path MakeCommandLogPath()
        {
            return std::filesystem::temp_directory_path() / "oiv-image-compat-command.log";
        }

        CommandResult RunCommandCapture(const std::vector<std::string>& args)
        {
            const auto logPath = MakeCommandLogPath();

            std::string command;
            for (const auto& arg : args)
            {
                if (!command.empty())
                    command += ' ';
                command += QuoteArgument(arg);
            }
            command += " > " + QuoteArgument(NarrowPath(logPath)) + " 2>&1";

#ifdef _WIN32
            command = "\"" + command + "\"";
#endif

            const int exitCode = std::system(command.c_str());

            std::ifstream log(logPath, std::ios::binary);
            std::ostringstream output;
            output << log.rdbuf();
            return CommandResult{exitCode, output.str()};
        }

        std::vector<std::string> SplitPathList(const char* pathList)
        {
            std::vector<std::string> result;
            if (pathList == nullptr)
                return result;

            std::string current;
            for (const char ch : std::string_view(pathList))
            {
                if (ch == ';')
                {
                    if (!current.empty())
                        result.push_back(current);
                    current.clear();
                }
                else
                {
                    current.push_back(ch);
                }
            }

            if (!current.empty())
                result.push_back(current);

            return result;
        }

        std::optional<std::filesystem::path> FindMagickOnPath()
        {
            const auto pathEntries                        = SplitPathList(std::getenv("PATH"));
            const std::vector<std::string> candidateNames = {"magick.exe", "magick.com", "magick.bat", "magick.cmd"};

            for (auto entry : pathEntries)
            {
                if (entry.size() >= 2 && entry.front() == '"' && entry.back() == '"')
                    entry = entry.substr(1, entry.size() - 2);

                for (const auto& candidateName : candidateNames)
                {
                    const auto candidate = std::filesystem::path(entry) / candidateName;
                    std::error_code ec;
                    if (std::filesystem::is_regular_file(candidate, ec))
                        return std::filesystem::absolute(candidate, ec);
                }
            }

            return std::nullopt;
        }

        std::optional<MagickVersion> ParseMagickVersion(const std::string& versionOutput)
        {
            const std::regex versionRegex(R"(ImageMagick\s+(\d+)\.(\d+)\.(\d+)-(\d+))");
            std::smatch match;
            if (!std::regex_search(versionOutput, match, versionRegex))
                return std::nullopt;

            return MagickVersion{std::stoi(match[1].str()), std::stoi(match[2].str()), std::stoi(match[3].str()),
                                 std::stoi(match[4].str())};
        }

        bool IsVersionAtLeastRequired(const MagickVersion& found, const MagickVersion& required)
        {
            return std::tie(found.major, found.minor, found.patch, found.build) >=
                   std::tie(required.major, required.minor, required.patch, required.build);
        }

        std::map<std::string, std::string> ParseMagickFormatModes(const std::string& formatOutput)
        {
            std::map<std::string, std::string> modes;
            const std::regex lineRegex(R"(^\s*([A-Z0-9]+)\*?\s+\S+\s+([rw+-]{3}))");
            std::istringstream lines(formatOutput);
            std::string line;
            while (std::getline(lines, line))
            {
                std::smatch match;
                if (std::regex_search(line, match, lineRegex))
                    modes[match[1].str()] = match[2].str();
            }
            return modes;
        }

        std::vector<std::string> FindMissingRequiredFormats(const std::string& formatOutput,
                                                            const std::vector<std::string>& requiredFormats)
        {
            const auto formatModes = ParseMagickFormatModes(formatOutput);

            std::vector<std::string> missingFormats;
            for (const auto& format : requiredFormats)
            {
                const auto it = formatModes.find(format);
                if (it == formatModes.end() || it->second.size() < 2 || it->second[0] != 'r' || it->second[1] != 'w')
                    missingFormats.push_back(format);
            }

            return missingFormats;
        }

        std::string JoinStrings(const std::vector<std::string>& values, std::string_view separator)
        {
            std::ostringstream joined;
            for (std::size_t i = 0; i < values.size(); ++i)
            {
                if (i != 0)
                    joined << separator;
                joined << values[i];
            }
            return joined.str();
        }

        std::string JsonScalarToString(const json& value)
        {
            if (value.is_string())
                return value.get<std::string>();
            if (value.is_number_integer())
                return std::to_string(value.get<long long>());
            if (value.is_number_unsigned())
                return std::to_string(value.get<unsigned long long>());
            if (value.is_number_float())
                return value.dump();
            if (value.is_boolean())
                return value.get<bool>() ? "true" : "false";

            FAIL("ImageMagick corpus config contains a non-scalar placeholder value: " << value.dump());
            return {};
        }

        std::string RequireString(const json& object, std::string_view field)
        {
            REQUIRE(object.contains(field));
            REQUIRE(object.at(field).is_string());
            return object.at(field).get<std::string>();
        }

        bool OptionalBool(const json& object, std::string_view field, bool defaultValue)
        {
            if (!object.contains(field))
                return defaultValue;
            REQUIRE(object.at(field).is_boolean());
            return object.at(field).get<bool>();
        }

        std::string OptionalString(const json& object, std::string_view field, std::string defaultValue = {})
        {
            if (!object.contains(field))
                return defaultValue;
            return JsonScalarToString(object.at(field));
        }

        std::vector<std::string> OptionalStringList(const json& object, std::string_view field)
        {
            std::vector<std::string> values;
            if (!object.contains(field))
                return values;

            REQUIRE(object.at(field).is_array());
            for (const auto& value : object.at(field))
                values.push_back(JsonScalarToString(value));
            return values;
        }

        CorpusConfig LoadCorpusConfig()
        {
            const auto configPath = GetCorpusConfigPath();
            std::ifstream configFile(configPath, std::ios::binary);
            REQUIRE(configFile.is_open());

            std::ostringstream rawContent;
            rawContent << configFile.rdbuf();

            try
            {
                return CorpusConfig{json::parse(rawContent.str()), rawContent.str()};
            }
            catch (const json::parse_error& error)
            {
                FAIL("Failed to parse ImageMagick corpus config at " << NarrowPath(configPath) << ": " << error.what());
                return {};
            }
        }

        RequiredMagick ReadRequiredMagick(const json& config)
        {
            REQUIRE(config.contains("requiredMagick"));
            const auto& required = config.at("requiredMagick");
            REQUIRE(required.is_object());

            RequiredMagick result;
            result.display = RequireString(required, "display");

            REQUIRE(required.contains("version"));
            REQUIRE(required.at("version").is_array());
            REQUIRE(required.at("version").size() == 4);
            result.version = MagickVersion{required.at("version").at(0).get<int>(),
                                           required.at("version").at(1).get<int>(),
                                           required.at("version").at(2).get<int>(),
                                           required.at("version").at(3).get<int>()};

            REQUIRE(required.contains("formats"));
            REQUIRE(required.at("formats").is_array());
            for (const auto& format : required.at("formats"))
                result.formats.push_back(JsonScalarToString(format));

            return result;
        }

        MagickEnvironment RequireMagickOrSkip(const RequiredMagick& required)
        {
            const auto magickPath = FindMagickOnPath();
            if (!magickPath.has_value())
            {
                SKIP(
                    "Skipping ImageMagick compatibility tests because 'magick' was not found on PATH. Required minimum "
                    "is "
                    << required.display
                    << ". Install ImageMagick 7 and ensure magick.exe, not Windows convert.exe, is available on PATH.");
            }

            const auto versionResult = RunCommandCapture({NarrowPath(*magickPath), "-version"});
            if (versionResult.exitCode != 0)
            {
                SKIP("Skipping ImageMagick compatibility tests because 'magick -version' failed for "
                     << NarrowPath(*magickPath) << ". Output: " << versionResult.output);
            }

            const auto parsedVersion = ParseMagickVersion(versionResult.output);
            if (!parsedVersion.has_value())
            {
                SKIP(
                    "Skipping ImageMagick compatibility tests because the ImageMagick version could not be parsed from "
                    << NarrowPath(*magickPath) << ". Required minimum is " << required.display
                    << ". Output: " << versionResult.output);
            }

            if (!IsVersionAtLeastRequired(*parsedVersion, required.version))
            {
                SKIP("Skipping ImageMagick compatibility tests because the ImageMagick version is older than required "
                     "at "
                     << NarrowPath(*magickPath) << ". Required minimum is " << required.display
                     << ". Found output: " << versionResult.output);
            }

            const auto formatResult = RunCommandCapture({NarrowPath(*magickPath), "-list", "format"});
            if (formatResult.exitCode != 0)
            {
                SKIP("Skipping ImageMagick compatibility tests because 'magick -list format' failed for "
                     << NarrowPath(*magickPath) << ". Output: " << formatResult.output);
            }

            const auto missingFormats = FindMissingRequiredFormats(formatResult.output, required.formats);
            if (!missingFormats.empty())
            {
                SKIP("Skipping ImageMagick compatibility tests because ImageMagick is missing required formats at "
                     << NarrowPath(*magickPath) << ". Missing formats: " << JoinStrings(missingFormats, ", ")
                     << ". Required minimum is " << required.display << ".");
            }

            return MagickEnvironment{*magickPath, versionResult.output, formatResult.output};
        }

        void RunMagick(const MagickEnvironment& magick, const std::vector<std::string>& args)
        {
            std::vector<std::string> commandArgs{NarrowPath(magick.executablePath)};
            commandArgs.insert(commandArgs.end(), args.begin(), args.end());

            const auto result = RunCommandCapture(commandArgs);
            INFO("ImageMagick command output:\n" << result.output);
            REQUIRE(result.exitCode == 0);
        }

        std::filesystem::path GetTestExecutableFolder()
        {
#ifdef _WIN32
            LLUtils::native_string_type buffer(MAX_PATH, LLUTILS_TEXT('\0'));
            DWORD length = 0;
            while (true)
            {
                length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
                if (length == 0)
                    break;

                if (length < buffer.size() - 1)
                    return std::filesystem::path(buffer.substr(0, length)).parent_path();

                buffer.resize(buffer.size() * 2);
            }
#endif
            return std::filesystem::current_path();
        }

        std::filesystem::path GetImageCompatibilityCacheFolder()
        {
            return GetTestExecutableFolder() / "oiv-image-compat-cache";
        }

        std::uint64_t HashString(std::string_view value)
        {
            std::uint64_t hash = 14695981039346656037ull;
            for (const unsigned char ch : value)
            {
                hash ^= ch;
                hash *= 1099511628211ull;
            }
            return hash;
        }

        std::string BuildManifestContent(const CorpusConfig& config)
        {
            std::ostringstream content;
            content << GeneratorSchemaVersion << "\n" << config.rawContent;

            std::ostringstream manifest;
            manifest << GeneratorSchemaVersion << ":" << std::hex << std::setw(16) << std::setfill('0')
                     << HashString(content.str());
            return manifest.str();
        }

        bool IsManifestCurrent(const std::filesystem::path& manifestPath, const std::string& expectedContent)
        {
            std::ifstream manifest(manifestPath, std::ios::binary);
            std::ostringstream content;
            content << manifest.rdbuf();
            return content.str() == expectedContent;
        }

        void WriteManifest(const std::filesystem::path& manifestPath, const std::string& content)
        {
            std::ofstream manifest(manifestPath, std::ios::binary);
            manifest << content;
        }

        void AddGeneratedExtension(GeneratedCorpus& corpus, const std::filesystem::path& path)
        {
            auto extension = path.extension().native();
            if (!extension.empty() && extension.front() == LLUTILS_TEXT('.'))
                extension.erase(extension.begin());
            extension = LLUtils::StringUtility::ToLower(extension);
            if (!extension.empty())
                corpus.extensions.insert(extension);
        }

        void FinalizeExtensionList(GeneratedCorpus& corpus)
        {
            LLUtils::native_stringstream extensions;
            bool first = true;
            for (const auto& extension : corpus.extensions)
            {
                if (!first)
                    extensions << LLUTILS_TEXT(";");
                first = false;
                extensions << extension;
            }
            corpus.extensionList = extensions.str();
        }

        void AddImage(GeneratedCorpus& corpus, std::string format, std::string variant,
                      const std::filesystem::path& path, std::string expectedLoadStatus = "loaded",
                      std::string expectedTexelFormat = {}, std::vector<std::string> expectedSubImageTexelFormats = {})
        {
            corpus.validImages.push_back(GeneratedImage{std::move(format), std::move(variant), path,
                                                        std::move(expectedLoadStatus), std::move(expectedTexelFormat),
                                                        std::move(expectedSubImageTexelFormats)});
        }

        void AddBadImage(GeneratedCorpus& corpus, std::string format, std::string variant,
                         const std::filesystem::path& path)
        {
            corpus.badImages.push_back(GeneratedImage{std::move(format), std::move(variant), path});
        }

        void WriteBinaryFile(const std::filesystem::path& path, std::string_view data)
        {
            std::filesystem::create_directories(path.parent_path());
            std::ofstream file(path, std::ios::binary);
            file.write(data.data(), static_cast<std::streamsize>(data.size()));
        }

        int HexValue(char ch)
        {
            if (ch >= '0' && ch <= '9')
                return ch - '0';
            if (ch >= 'a' && ch <= 'f')
                return ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F')
                return ch - 'A' + 10;
            FAIL("Invalid hex character in ImageMagick corpus bad-file data");
            return 0;
        }

        std::string DecodeHexString(const std::string& text)
        {
            std::string hex;
            std::ranges::copy_if(text, std::back_inserter(hex),
                                 [](const unsigned char ch) { return !std::isspace(ch); });
            REQUIRE(hex.size() % 2 == 0);

            std::string data;
            data.reserve(hex.size() / 2);
            for (std::size_t i = 0; i < hex.size(); i += 2)
                data.push_back(static_cast<char>((HexValue(hex[i]) << 4) | HexValue(hex[i + 1])));
            return data;
        }

        std::pair<int, int> ParseSize(std::string_view size)
        {
            const auto delimiter = size.find('x');
            REQUIRE(delimiter != std::string_view::npos);
            const auto width  = std::stoi(std::string(size.substr(0, delimiter)));
            const auto height = std::stoi(std::string(size.substr(delimiter + 1)));
            return {width, height};
        }

        void GenerateComplexBaseImage(const MagickEnvironment& magick, const std::filesystem::path& sourcePath,
                                      const ValueMap& variables)
        {
            const auto seedIt          = variables.find("seed");
            const auto baseSizeIt      = variables.find("baseSize");
            const auto seed            = seedIt == variables.end() ? std::string("314159") : seedIt->second;
            const auto baseSize        = baseSizeIt == variables.end() ? std::string("319x241") : baseSizeIt->second;
            const auto [width, height] = ParseSize(baseSize);
            const auto maxX            = width - 1;
            const auto maxY            = height - 1;

            std::vector<std::string> args = {
                "-seed",
                seed,
                "-size",
                baseSize,
                "plasma:fractal",
                "-colorspace",
                "sRGB",
                "-alpha",
                "set",
                "-stroke",
                "rgba(255,255,255,0.85)",
                "-strokewidth",
                "1",
                "-draw",
                "line 0,0 " + std::to_string(maxX) + "," + std::to_string(maxY) + " line " + std::to_string(maxX) +
                    ",0 0," + std::to_string(maxY),
                "-draw",
                "line 0,120 " + std::to_string(maxX) + ",120 line 159,0 159," + std::to_string(maxY),
                "-fill",
                "rgba(255,255,255,0.52)",
                "-draw",
                "rectangle 12,12 124,72",
                "-fill",
                "rgba(0,0,0,0.74)",
                "-draw",
                "rectangle 16,16 120,68",
                "-fill",
                "white",
                "-pointsize",
                "18",
                "-draw",
                "text 24,42 'OIV'",
                "-pointsize",
                "10",
                "-draw",
                "text 24,60 'codec stress'",
                "-fill",
                "rgba(255,0,0,0.45)",
                "-draw",
                "circle 78,148 118,148",
                "-fill",
                "rgba(0,255,255,0.42)",
                "-draw",
                "polygon 204,34 294,92 228,160",
                "-fill",
                "rgba(255,255,0,0.38)",
                "-draw",
                "roundrectangle 198,168 304,224 8,8",
                "-fill",
                "rgba(0,0,0,0.0)",
                "-stroke",
                "rgba(0,0,0,0.95)",
                "-strokewidth",
                "2",
                "-draw",
                "rectangle 0,0 " + std::to_string(maxX) + "," + std::to_string(maxY),
                "-stroke",
                "rgba(255,0,255,0.80)",
                "-strokewidth",
                "1",
                "-draw",
                "line 24,92 132,230 line 32,92 140,230 line 40,92 148,230",
                "-fill",
                "red",
                "-draw",
                "rectangle 0,0 12,12",
                "-fill",
                "lime",
                "-draw",
                "rectangle " + std::to_string(maxX - 12) + ",0 " + std::to_string(maxX) + ",12",
                "-fill",
                "blue",
                "-draw",
                "rectangle 0," + std::to_string(maxY - 12) + " 12," + std::to_string(maxY),
                "-fill",
                "yellow",
                "-draw",
                "rectangle " + std::to_string(maxX - 12) + "," + std::to_string(maxY - 12) + " " +
                    std::to_string(maxX) + "," + std::to_string(maxY)};

            for (int x = 0; x < 96; x += 12)
            {
                for (int y = 0; y < 48; y += 12)
                {
                    const bool light = ((x + y) / 12) % 2 == 0;
                    args.push_back("-fill");
                    args.push_back(light ? "rgba(255,255,255,0.88)" : "rgba(0,0,0,0.88)");
                    args.push_back("-draw");
                    args.push_back("rectangle " + std::to_string(176 + x) + "," + std::to_string(12 + y) + " " +
                                   std::to_string(187 + x) + "," + std::to_string(23 + y));
                }
            }

            args.push_back(NarrowPath(sourcePath));
            RunMagick(magick, args);
        }

        ValueMap ReadVariables(const json& config)
        {
            ValueMap variables;
            if (!config.contains("variables"))
                return variables;

            REQUIRE(config.at("variables").is_object());
            for (const auto& [key, value] : config.at("variables").items())
                variables[key] = JsonScalarToString(value);
            return variables;
        }

        ContextSet BuildLoopContexts(const json& command)
        {
            if (!command.contains("forEach"))
                return ContextSet{ValueMap{}};

            const auto& forEach = command.at("forEach");
            REQUIRE(forEach.is_object());
            REQUIRE(forEach.size() == 1);

            const auto loopName = forEach.begin().key();
            const auto& values  = forEach.begin().value();
            REQUIRE(values.is_array());

            ContextSet contexts;
            for (const auto& value : values)
            {
                ValueMap context;
                if (value.is_object())
                {
                    for (const auto& [key, field] : value.items())
                        context[key] = JsonScalarToString(field);
                }
                else
                {
                    context[loopName] = JsonScalarToString(value);
                }
                contexts.push_back(std::move(context));
            }
            return contexts;
        }

        std::string PathOutput(const std::filesystem::path& path)
        {
            return NarrowPath(path);
        }

        std::string ResolveToken(std::string_view token, const ValueMap& variables, const ValueMap& context,
                                 const OutputMap& outputs)
        {
            const auto tokenString = std::string(token);
            if (const auto it = context.find(tokenString); it != context.end())
                return it->second;
            if (const auto it = variables.find(tokenString); it != variables.end())
                return it->second;
            if (tokenString == "source")
            {
                const auto sourceIt = outputs.find("source");
                REQUIRE(sourceIt != outputs.end());
                REQUIRE(!sourceIt->second.empty());
                return PathOutput(sourceIt->second.front());
            }
            if (const auto outputIt = outputs.find(tokenString); outputIt != outputs.end())
            {
                REQUIRE(!outputIt->second.empty());
                return PathOutput(outputIt->second.front());
            }

            FAIL("Unknown ImageMagick corpus placeholder: {" << tokenString << "}");
            return {};
        }

        std::string ExpandTemplate(std::string_view value, const ValueMap& variables, const ValueMap& context,
                                   const OutputMap& outputs)
        {
            std::string expanded;
            std::size_t cursor = 0;
            while (cursor < value.size())
            {
                const auto open = value.find('{', cursor);
                if (open == std::string_view::npos)
                {
                    expanded.append(value.substr(cursor));
                    break;
                }

                expanded.append(value.substr(cursor, open - cursor));
                const auto close = value.find('}', open + 1);
                REQUIRE(close != std::string_view::npos);
                expanded += ResolveToken(value.substr(open + 1, close - open - 1), variables, context, outputs);
                cursor = close + 1;
            }
            return expanded;
        }

        std::optional<std::vector<std::string>> TryExpandWildcardArgument(std::string_view value,
                                                                          const OutputMap& outputs)
        {
            if (value.size() < 5 || value.front() != '{' || value.back() != '}')
                return std::nullopt;

            const std::string token(value.substr(1, value.size() - 2));
            constexpr std::string_view suffix = ":*";
            if (!token.ends_with(suffix))
                return std::nullopt;

            const auto outputId = token.substr(0, token.size() - suffix.size());
            const auto it       = outputs.find(outputId);
            REQUIRE(it != outputs.end());
            REQUIRE(!it->second.empty());

            std::vector<std::string> expanded;
            for (const auto& output : it->second)
                expanded.push_back(PathOutput(output));
            return expanded;
        }

        std::vector<std::string> ExpandArguments(const json& rawArgs, const ValueMap& variables,
                                                 const ValueMap& context, const OutputMap& outputs)
        {
            REQUIRE(rawArgs.is_array());

            std::vector<std::string> args;
            for (const auto& rawArg : rawArgs)
            {
                const auto arg = JsonScalarToString(rawArg);
                if (const auto wildcardArgs = TryExpandWildcardArgument(arg, outputs); wildcardArgs.has_value())
                {
                    args.insert(args.end(), wildcardArgs->begin(), wildcardArgs->end());
                    continue;
                }

                args.push_back(ExpandTemplate(arg, variables, context, outputs));
            }
            return args;
        }

        std::filesystem::path BuildOutputPath(const std::filesystem::path& cacheFolder, std::string_view output,
                                              const ValueMap& variables, const ValueMap& context,
                                              const OutputMap& outputs)
        {
            const auto expandedOutput = ExpandTemplate(output, variables, context, outputs);
            const std::filesystem::path relativePath(expandedOutput);
            REQUIRE(!relativePath.is_absolute());
            return cacheFolder / relativePath;
        }

        void ProcessSources(const json& config, const MagickEnvironment& magick, const ValueMap& variables,
                            GeneratedCorpus& corpus, OutputMap& outputs, bool regenerate)
        {
            REQUIRE(config.contains("sources"));
            REQUIRE(config.at("sources").is_array());

            for (const auto& source : config.at("sources"))
            {
                const auto id      = RequireString(source, "id");
                const auto output  = RequireString(source, "output");
                const auto kind    = RequireString(source, "kind");
                const auto outPath = BuildOutputPath(corpus.folder, output, variables, {}, outputs);
                outputs[id].push_back(outPath);

                if (regenerate)
                {
                    std::filesystem::create_directories(outPath.parent_path());
                    if (kind == "complexBaseImage")
                        GenerateComplexBaseImage(magick, outPath, variables);
                    else
                        FAIL("Unsupported ImageMagick corpus source kind: " << kind);
                }

                if (OptionalBool(source, "testImage", false))
                    AddImage(corpus, RequireString(source, "format"), RequireString(source, "variant"), outPath,
                             OptionalString(source, "expectedLoadStatus", "loaded"),
                             OptionalString(source, "expectedTexelFormat"),
                             OptionalStringList(source, "expectedSubImageTexelFormats"));
            }
        }

        void ProcessCommands(const json& config, const MagickEnvironment& magick, const ValueMap& variables,
                             GeneratedCorpus& corpus, OutputMap& outputs, bool regenerate)
        {
            REQUIRE(config.contains("commands"));
            REQUIRE(config.at("commands").is_array());

            for (const auto& command : config.at("commands"))
            {
                const auto id          = RequireString(command, "id");
                const auto output      = RequireString(command, "output");
                const auto testImage   = OptionalBool(command, "testImage", false);
                const auto loopContext = BuildLoopContexts(command);

                for (const auto& context : loopContext)
                {
                    const auto outPath         = BuildOutputPath(corpus.folder, output, variables, context, outputs);
                    ValueMap contextWithOutput = context;
                    contextWithOutput["out"]   = PathOutput(outPath);
                    outputs[id].push_back(outPath);

                    if (regenerate)
                    {
                        std::filesystem::create_directories(outPath.parent_path());
                        RunMagick(magick, ExpandArguments(command.at("args"), variables, contextWithOutput, outputs));
                    }

                    if (testImage)
                    {
                        AddImage(
                            corpus,
                            ExpandTemplate(RequireString(command, "format"), variables, contextWithOutput, outputs),
                            ExpandTemplate(RequireString(command, "variant"), variables, contextWithOutput, outputs),
                            outPath,
                            ExpandTemplate(OptionalString(command, "expectedLoadStatus", "loaded"), variables,
                                           contextWithOutput, outputs),
                            ExpandTemplate(OptionalString(command, "expectedTexelFormat"), variables, contextWithOutput,
                                           outputs),
                            OptionalStringList(command, "expectedSubImageTexelFormats"));
                    }
                }
            }
        }

        void ProcessBadFiles(const json& config, const ValueMap& variables, GeneratedCorpus& corpus,
                             const OutputMap& outputs, bool regenerate)
        {
            REQUIRE(config.contains("badFiles"));
            REQUIRE(config.at("badFiles").is_array());

            for (const auto& badFile : config.at("badFiles"))
            {
                const auto output  = RequireString(badFile, "output");
                const auto outPath = BuildOutputPath(corpus.folder, output, variables, {}, outputs);

                if (regenerate)
                {
                    if (badFile.contains("text"))
                        WriteBinaryFile(outPath, badFile.at("text").get<std::string>());
                    else if (badFile.contains("hex"))
                        WriteBinaryFile(outPath, DecodeHexString(badFile.at("hex").get<std::string>()));
                    else
                        FAIL("Bad ImageMagick corpus file is missing text or hex data: " << output);
                }

                AddBadImage(corpus, RequireString(badFile, "format"), RequireString(badFile, "variant"), outPath);
            }
        }

        void FinalizeCorpus(GeneratedCorpus& corpus)
        {
            for (const auto& image : corpus.validImages)
                AddGeneratedExtension(corpus, image.path);
            for (const auto& image : corpus.badImages)
                AddGeneratedExtension(corpus, image.path);
            FinalizeExtensionList(corpus);

            for (const auto& image : corpus.validImages)
            {
                INFO("Missing generated image: " << NarrowPath(image.path));
                REQUIRE(std::filesystem::is_regular_file(image.path));
            }
            for (const auto& image : corpus.badImages)
            {
                INFO("Missing generated bad image: " << NarrowPath(image.path));
                REQUIRE(std::filesystem::is_regular_file(image.path));
            }
        }

        GeneratedCorpus BuildGeneratedCorpus()
        {
            const auto config    = LoadCorpusConfig();
            const auto required  = ReadRequiredMagick(config.root);
            const auto magick    = RequireMagickOrSkip(required);
            const auto variables = ReadVariables(config.root);
            const auto manifest  = BuildManifestContent(config);
            GeneratedCorpus corpus;
            corpus.folder           = GetImageCompatibilityCacheFolder();
            const auto manifestPath = corpus.folder / "manifest.txt";
            const bool regenerate   = !IsManifestCurrent(manifestPath, manifest);

            if (regenerate)
            {
                std::filesystem::remove_all(corpus.folder);
                std::filesystem::create_directories(corpus.folder);
                std::filesystem::create_directories(corpus.folder / "work");
            }

            OutputMap outputs;
            ProcessSources(config.root, magick, variables, corpus, outputs, regenerate);
            ProcessCommands(config.root, magick, variables, corpus, outputs, regenerate);
            ProcessBadFiles(config.root, variables, corpus, outputs, regenerate);
            if (regenerate)
                WriteManifest(manifestPath, manifest);

            FinalizeCorpus(corpus);
            return corpus;
        }

        std::set<LLUtils::native_string_type> BuildBadFileSet(const GeneratedCorpus& corpus)
        {
            std::set<LLUtils::native_string_type> badFiles;
            for (const auto& image : corpus.badImages)
                badFiles.insert(image.path.native());
            return badFiles;
        }
    }  // namespace

    std::string NarrowPath(const std::filesystem::path& path)
    {
        return path.string();
    }

    const GeneratedCorpus& EnsureImageMagickCorpus()
    {
        static std::optional<GeneratedCorpus> corpus;
        if (!corpus.has_value())
            corpus = BuildGeneratedCorpus();
        return *corpus;
    }

    std::vector<LLUtils::native_string_type> BuildBrowsingFolderFileList(const GeneratedCorpus& corpus)
    {
        std::vector<LLUtils::native_string_type> result;
        for (const auto& image : corpus.validImages)
            result.push_back(image.path.native());
        for (const auto& image : corpus.badImages)
            result.push_back(image.path.native());

        OIV::FileSorter sorter;
        std::sort(result.begin(), result.end(), sorter);
        return result;
    }

    std::vector<LLUtils::native_string_type> FindConsecutiveValidFiles(const GeneratedCorpus& corpus, std::size_t count)
    {
        const auto allFiles = BuildBrowsingFolderFileList(corpus);
        const auto badFiles = BuildBadFileSet(corpus);

        for (auto first = allFiles.begin(); first != allFiles.end(); ++first)
        {
            std::vector<LLUtils::native_string_type> window;
            for (auto it = first; it != allFiles.end() && window.size() < count; ++it)
            {
                if (badFiles.contains(*it))
                    break;
                window.push_back(*it);
            }

            if (window.size() == count)
                return window;
        }

        FAIL("Could not find " << count << " consecutive valid ImageMagick corpus files");
        return {};
    }

    std::pair<LLUtils::native_string_type, LLUtils::native_string_type> FindValidFileBeforeBadFile(
        const GeneratedCorpus& corpus)
    {
        const auto allFiles = BuildBrowsingFolderFileList(corpus);
        const auto badFiles = BuildBadFileSet(corpus);

        for (std::size_t i = 1; i < allFiles.size(); ++i)
        {
            if (!badFiles.contains(allFiles[i - 1]) && badFiles.contains(allFiles[i]))
                return {allFiles[i - 1], allFiles[i]};
        }

        FAIL("Could not find a valid ImageMagick corpus file immediately before a bad file");
        return {};
    }
}  // namespace OIV::Tests
