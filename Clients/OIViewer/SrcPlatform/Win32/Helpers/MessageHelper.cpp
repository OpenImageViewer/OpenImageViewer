#include "MessageHelper.h"
#include "MessageFormatter.h"
#include <OIVAppCore/ImageInfoPresentationPolicy.h>
#include "../ConfigurationLoader.h"
#include <ImageCodec.h>

#include <algorithm>

namespace OIV
{
    LLUtils::native_string_type MessageHelper::CreateKeyBindingsMessage()
    {
        using namespace std;
        //string message = DefaultHeaderColor + "Image information\n";

        MessageFormatter::FormatArgs args;
        args.keyColor = MessageFormatter::DefaultKeyColor;
        args.maxLines = 24;
        args.minSpaceFromValue = 3;
        args.spacer = '.';
        args.valueColor = MessageFormatter::DefaultValueColor;
        args.spaceBetweenColumns = 3;
        MessageFormatter::MessagesValues& messageValues = args.messageValues;


        auto keybindingsList = ConfigurationLoader::LoadKeyBindings();
        auto commands = ConfigurationLoader::LoadCommandGroups();

        for (const auto& binding : keybindingsList)
        {
            auto it = std::find_if(commands.begin(), commands.end(), [&](auto& element)->bool
                {
                    return element.commandGroupID == binding.GroupID;
                });

            if (it != commands.end())
                messageValues.emplace_back(binding.KeyCombinationName, MessageFormatter::ValueObjectList{ {it->commandDisplayName} });
        }

        return MessageFormatter::FormatMetaText(args);
    }

    LLUtils::native_string_type MessageHelper::GetFileTime(const LLUtils::native_string_type& filePath)
    {
        return ImageInfoPresentationPolicy::FormatFileTime(filePath);
    }

    LLUtils::native_string_type MessageHelper::CreateImageInfoMessage(const OIVBaseImageSharedPtr& oivImage, const OIVBaseImageSharedPtr& rasterized, IMCodec::ImageCodec& imageCodec)
    {
        LLUtils::native_string_type message = MessageFormatter::DefaultHeaderColor + LLUTILS_TEXT("Image information\n");

        MessageFormatter::FormatArgs args;
        args.keyColor = MessageFormatter::DefaultKeyColor;
        args.maxLines = 24;
        args.minSpaceFromValue = 3;
        args.spacer = '.';
        args.valueColor = LLUTILS_TEXT("<textcolor=#ffffff>");
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

        message += L'\n' + MessageFormatter::FormatMetaText(args);
        return message;
    }
}
