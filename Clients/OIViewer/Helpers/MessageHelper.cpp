#include "MessageHelper.h"
#include "MessageFormatter.h"
#include "PixelHelper.h"
#include "../OIVImage/OIVFileImage.h"
#include "../ConfigurationLoader.h"
namespace OIV
{
    std::wstring  MessageHelper::ParseImageSource(const OIVBaseImageSharedPtr& image)
    {
        switch (image->GetDescriptor().Source)
        {
        case ImageSource::None:
            return L"none";
        case ImageSource::File:
            return L"file";
        case ImageSource::Clipboard:
            return L"clipboard";
        case ImageSource::InternalText:
            return L"internal text";
        case ImageSource::GeneratedByLib:
            return L"auto generated";
        default:
            return L"unknown";
        }
    }
    
    std::wstring MessageHelper::CreateKeyBindingsMessage()
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
                    messageValues.emplace_back(binding.KeyCombinationName, MessageFormatter::ValueObjectList{ it->commandDisplayName });
        }

        return MessageFormatter::FormatMetaText(args);
    }


    std::wstring  MessageHelper::CreateImageInfoMessage(const OIVBaseImageSharedPtr& image)
    {
        using namespace std;
        wstring message = MessageFormatter::DefaultHeaderColor + L"Image information\n";

        MessageFormatter::FormatArgs args;
        args.keyColor = MessageFormatter::DefaultKeyColor;
        args.maxLines = 24;
        args.minSpaceFromValue = 3;
        args.spacer = '.';
        args.valueColor = L"<textcolor=#ffffff>";
        MessageFormatter::MessagesValues& messageValues = args.messageValues;


        if (image->GetDescriptor().Source != ImageSource::File)
            messageValues.emplace_back("Source", MessageFormatter::ValueObjectList{ ParseImageSource(image) });
        else
            messageValues.emplace_back("File path", MessageFormatter::ValueObjectList{ 
                MessageFormatter::FormatFilePath(std::dynamic_pointer_cast<OIVFileImage>(image)->GetFileName()) });

        const auto& texelInfo = IMCodec::GetTexelInfo(static_cast<IMCodec::TexelFormat>(image->GetDescriptor().texelFormat));

        messageValues.emplace_back("Width", MessageFormatter::ValueObjectList{ image->GetDescriptor().Width , "px" });
        messageValues.emplace_back("Height", MessageFormatter::ValueObjectList{ image->GetDescriptor().Height , "px" });
        messageValues.emplace_back("bit depth", MessageFormatter::ValueObjectList{ image->GetDescriptor().Bpp , " bpp" });
        messageValues.emplace_back("channels info", MessageFormatter::ValueObjectList{ MessageFormatter::FormatTexelInfo(texelInfo) });
        messageValues.emplace_back("Num sub-images", MessageFormatter::ValueObjectList{ image->GetDescriptor().NumSubImages });
        messageValues.emplace_back("Load time",  MessageFormatter::ValueObjectList{ static_cast<long double>(image->GetDescriptor().LoadTime) , "ms" });
        messageValues.emplace_back("Display time", MessageFormatter::ValueObjectList{ image->GetDescriptor().DisplayTime , "ms" });
        messageValues.emplace_back("Codec used", MessageFormatter::ValueObjectList{ image->GetDescriptor().pluginUsed != nullptr ? image->GetDescriptor().pluginUsed : "N/A" });
        
        auto uniqueValues = PixelHelper::CountUniqueValues(image);
        if (uniqueValues > -1)
            messageValues.emplace_back("Unique values", MessageFormatter::ValueObjectList{ uniqueValues });

        message += L'\n' + MessageFormatter::FormatMetaText(args);
        return message;
    }
}