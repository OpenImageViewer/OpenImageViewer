#include <OIVAppCore/MessageHelper.h>
#include <OIVAppCore/MessageFormatter.h>
#include <OIVAppCore/ImageInfoPresentationPolicy.h>
#include <OIVAppCore/ConfigurationLoader.h>
#include <ImageCodec.h>

#include <Interfaces/IRenderer.h>
#include <LLUtils/PlatformUtility.h>

#include <algorithm>
#include <cstdio>
#include <fstream>
#include <format>
#include <array>
#include <memory>

namespace OIV
{
    LLUtils::native_string_type MessageHelper::CreateKeyBindingsMessage()
    {
        using namespace std;
        // string message = DefaultHeaderColor + "Image information\n";

        MessageFormatter::FormatArgs args;
        args.keyColor                                   = MessageFormatter::DefaultKeyColor;
        args.maxLines                                   = 24;
        args.minSpaceFromValue                          = 3;
        args.spacer                                     = '.';
        args.valueColor                                 = MessageFormatter::DefaultValueColor;
        args.spaceBetweenColumns                        = 3;
        MessageFormatter::MessagesValues& messageValues = args.messageValues;

        auto keybindingsList = ConfigurationLoader::LoadKeyBindings();
        auto commands        = ConfigurationLoader::LoadCommandGroups();

        for (const auto& binding : keybindingsList)
        {
            auto it = std::find_if(commands.begin(), commands.end(),
                                   [&](auto& element) -> bool { return element.commandGroupID == binding.GroupID; });

            if (it != commands.end())
                messageValues.emplace_back(binding.KeyCombinationName,
                                           MessageFormatter::ValueObjectList{{it->commandDisplayName}});
        }

        return MessageFormatter::FormatMetaText(args);
    }

    LLUtils::native_string_type MessageHelper::GetFileTime(const LLUtils::native_string_type& filePath)
    {
        return ImageInfoPresentationPolicy::FormatFileTime(filePath);
    }

    LLUtils::native_string_type MessageHelper::CreateImageInfoMessage(const OIVBaseImageSharedPtr& oivImage,
                                                                      const OIVBaseImageSharedPtr& rasterized,
                                                                      IMCodec::ImageCodec& imageCodec)
    {
        LLUtils::native_string_type message = MessageFormatter::DefaultHeaderColor +
                                              LLUTILS_TEXT("Image information\n");

        MessageFormatter::FormatArgs args;
        args.keyColor                                   = MessageFormatter::DefaultKeyColor;
        args.maxLines                                   = 24;
        args.minSpaceFromValue                          = 3;
        args.spacer                                     = '.';
        args.valueColor                                 = LLUTILS_TEXT("<textcolor=#ffffff>");
        MessageFormatter::MessagesValues& messageValues = args.messageValues;

        const auto rows = ImageInfoPresentationPolicy::Build(oivImage, rasterized, imageCodec);
        for (const auto& row : rows)
        {
            MessageFormatter::ValueObjectList values;
            values.reserve(row.values.size());

            if (row.key == "File path" && row.values.empty() == false)
            {
                values.emplace_back(MessageFormatter::FormatFilePath(row.values.front().text));
            }
            else
            {
                for (const auto& value : row.values)
                    values.emplace_back(value.text);
            }

            messageValues.emplace_back(row.key, values);
        }

        message += LLUTILS_TEXT('\n');
        message += MessageFormatter::FormatMetaText(args);
        return message;
    }

    LLUtils::native_string_type MessageHelper::CreateSystemInfoMessage(const SystemInfoContext& context)
    {
        std::string osName;
#if LLUTILS_PLATFORM == LLUTILS_PLATFORM_WIN32
        try
        {
            LLUtils::PlatformUtility::OSVersion ver = LLUtils::PlatformUtility::GetOSVersion();
            osName                                  = std::format("Windows {}.{}.{}", ver.major, ver.minor, ver.build);
        }
        catch (...)
        {
            osName = "Windows";
        }
#else
        {
            std::ifstream osRelease("/etc/os-release");
            if (osRelease.is_open())
            {
                std::string line;
                while (std::getline(osRelease, line))
                {
                    if (line.starts_with("PRETTY_NAME="))
                    {
                        std::string prettyName = line.substr(12);
                        if (!prettyName.empty() && prettyName.front() == '"' && prettyName.back() == '"')
                            prettyName = prettyName.substr(1, prettyName.size() - 2);
                        else if (!prettyName.empty() && prettyName.front() == '\'')
                            prettyName = prettyName.substr(1, prettyName.size() - 2);
                        osName = std::move(prettyName);
                        break;
                    }
                }
            }
            if (osName.empty())
            {
                std::array<char, 128> buffer;
                std::string result;
                std::unique_ptr<FILE, decltype(&pclose)> pipe(popen("uname -srmo", "r"), pclose);
                if (pipe)
                {
                    while (!feof(pipe.get()))
                    {
                        if (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
                            result += buffer.data();
                    }
                    result = LLUtils::StringUtility::rtrim(result, "\n");
                    osName = std::move(result);
                }
            }
            if (osName.empty())
                osName = "Linux";
        }
#endif

        const auto coresInfo = LLUtils::PlatformUtility::GetCPUCoresInfo();
        const auto cpuCores  = std::format("{} physical / {} logical", coresInfo.physicalCores, coresInfo.logicalCores);
        const auto* renderer = context.renderer;
        const int adapterIndex = renderer ? renderer->GetSelectedGPUIndex() : -1;
        const auto indexText   = adapterIndex < 0 ? std::string{} : std::to_string(adapterIndex);

        LLUtils::native_string_type message = MessageFormatter::DefaultHeaderColor +
                                              LLUTILS_TEXT("System information\n");

        MessageFormatter::FormatArgs args;
        args.keyColor                                   = MessageFormatter::DefaultKeyColor;
        args.maxLines                                   = 24;
        args.minSpaceFromValue                          = 3;
        args.spacer                                     = '.';
        args.valueColor                                 = MessageFormatter::DefaultValueColor;
        args.spaceBetweenColumns                        = 3;
        MessageFormatter::MessagesValues& messageValues = args.messageValues;

        // One value per labeled row: the formatter's columns concatenate within a row.
        for (const auto& [label, value] : std::initializer_list<std::pair<const char*, std::string_view>>{
                 {"Application", context.appName},
                 {"Version", context.appVersion},
                 {"Build", context.buildType},
                 {"Commit", context.gitHash},
                 {"Operating system", osName},
                 {"CPU cores", cpuCores},
                 {"Renderer", renderer ? renderer->GetBackendName() : "Unknown"},
                 {"Adapter", renderer ? renderer->GetGPUName() : "Unknown"},
                 {"Adapter index", indexText},
                 {"Acceleration", GetAccelerationName(renderer ? renderer->GetAcceleration() : Acceleration::Unknown)},
                 {"API version", renderer ? renderer->GetAPIVersion() : "Unknown"},
                 {"Driver version", renderer ? renderer->GetDriverVersion() : "Unknown"}})
        {
            messageValues.emplace_back(label, MessageFormatter::ValueObjectList{MessageFormatter::ValueObject(
                                                  LLUtils::StringUtility::ConvertString<LLUtils::native_string_type>(
                                                      value.empty() ? std::string_view{"Not reported"} : value))});
        }

        message += LLUTILS_TEXT('\n');
        message += MessageFormatter::FormatMetaText(args);
        return message;
    }
}  // namespace OIV
