#include "MessageHelper.h"
#include "MessageFormatter.h"
#include "PixelHelper.h"
#include  <OIVImage/OIVFileImage.h>
#include "../ConfigurationLoader.h"
#include "UnitsHelper.h"
#include <ImageCodec.h>
namespace OIV
{
    std::wstring  MessageHelper::ParseImageSource(const OIVBaseImageSharedPtr& image)
    {
        switch (image->GetImageSource())
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
                messageValues.emplace_back(binding.KeyCombinationName, MessageFormatter::ValueObjectList{ {it->commandDisplayName} });
        }

        return MessageFormatter::FormatMetaText(args);
    }

    std::wstring MessageHelper::GetFileTime(const std::wstring& filePath)
    {
        auto fileTime = std::filesystem::last_write_time(filePath);
        std::chrono::system_clock::time_point systemTime;

#ifdef  __GNUC__
        systemTime = std::chrono::file_clock::to_sys(fileTime);
#elif defined(WIN32) && !defined(__MINGW32__)
        // In Windows 10 Creators Update, ICU was integrated into Windows, making the C APIs and data publicly accessible.
        // Since Windows 10 Build 1703 Microsft had integrated ICU libraries into the Windows System which caused backwars compatabilty as the DLL is missing in older systems.
        // https://docs.microsoft.com/en-us/windows/win32/intl/international-components-for-unicode--icu-
        if (auto osVersion = LLUtils::PlatformUtility::GetOSVersion(); osVersion.major > 10 || (osVersion.major == 10 && osVersion.build >= 15063 /*Version 1703*/))
        {
            systemTime = std::chrono::clock_cast<std::chrono::system_clock>(fileTime);
        }
        else
        {
            auto ticks = fileTime.time_since_epoch().count() - std::filesystem::__std_fs_file_time_epoch_adjustment;
            systemTime = std::chrono::system_clock::time_point(std::chrono::system_clock::duration(ticks));
        }
#endif

        auto in_time_t = std::chrono::system_clock::to_time_t(systemTime);
        std::wstringstream ss;
        tm tmDest;
        errno_t errorCode = localtime_s(&tmDest, &in_time_t) != 0;
        if (errorCode != 0)
        {
            using namespace std::string_literals;
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "could not convert time_t to a tm structure, error code: "s + std::to_string(errorCode));
        }
        ss << std::put_time(&tmDest, OIV_TEXT("%Y-%m-%d %X"));
        return ss.str();
    }

    std::wstring MessageHelper::CreateImageInfoMessage(const OIVBaseImageSharedPtr& oivImage, const OIVBaseImageSharedPtr& rasterized, IMCodec::ImageCodec& imageCodec)
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

        std::shared_ptr<OIVFileImage> fileImage = std::dynamic_pointer_cast<OIVFileImage>(oivImage);

        if (fileImage != nullptr)
        {
            auto const& filePath = fileImage->GetFileName();

            if (fileImage->GetImageSource() != ImageSource::File)
                messageValues.emplace_back("Source", MessageFormatter::ValueObjectList{ { ParseImageSource(fileImage) } });
            else
                messageValues.emplace_back("File path", MessageFormatter::ValueObjectList{ {MessageFormatter::FormatFilePath(filePath) } });

            auto fileSize = std::filesystem::file_size(filePath);

            messageValues.emplace_back("File size", MessageFormatter::ValueObjectList{ { UnitHelper::FormatUnit(fileSize,UnitType::BinaryDataShort,0,0) } });
            messageValues.emplace_back("File date", MessageFormatter::ValueObjectList{ { GetFileTime(filePath) } });
            auto bitmapSize = rasterized->GetImage()->GetTotalSizeOfImageTexels();
            auto compressionRatio = static_cast<double>(bitmapSize) / static_cast<double>(fileSize);
            messageValues.emplace_back("Compression ratio", MessageFormatter::ValueObjectList{ {L"1:"} ,{compressionRatio } });
        }

        const bool isAnimation = rasterized->GetImage()->GetItemType() == IMCodec::ImageItemType::Container
            && rasterized->GetImage()->GetSubImageGroupType() == IMCodec::ImageItemType::AnimationFrame;


        if (isAnimation)
            messageValues.emplace_back("Num frames", MessageFormatter::ValueObjectList{ { rasterized->GetImage()->GetNumSubImages()} });
        else
            messageValues.emplace_back("Num sub-images", MessageFormatter::ValueObjectList{ {rasterized->GetImage()->GetNumSubImages()} });


        auto frame = isAnimation ? rasterized->GetImage()->GetSubImage(0) : rasterized->GetImage();


        messageValues.emplace_back("Width", MessageFormatter::ValueObjectList{ { frame->GetWidth()} ,{ "px" } });
        messageValues.emplace_back("Height", MessageFormatter::ValueObjectList{ {frame->GetHeight() }, {"px"} });
        messageValues.emplace_back("bit depth", MessageFormatter::ValueObjectList{ {frame->GetBitsPerTexel()} , {" bpp"} });
        messageValues.emplace_back("channels info", MessageFormatter::ValueObjectList{ { MessageFormatter::FormatTexelInfo(frame->GetTexelInfo()) } });
        if (frame->GetOriginalTexelFormat() != IMCodec::TexelFormat::UNKNOWN
            && frame->GetOriginalTexelFormat() != frame->GetTexelFormat())
        {
            messageValues.emplace_back("original channels info", MessageFormatter::ValueObjectList{ {MessageFormatter::FormatTexelInfo(frame->GetOriginalTexelInfo()) } });
        }


        const auto& processData = frame->GetProcessData();
        messageValues.emplace_back("Load time", MessageFormatter::ValueObjectList{ {static_cast<long double>(processData.processTime)} , {"ms" } });
        messageValues.emplace_back("Display time", MessageFormatter::ValueObjectList{ { rasterized->GetDisplayTime()} , {"ms"} });
        std::wstring pluginDescription = L"Unknown";
        IMCodec::PluginProperties properties;
        if (imageCodec.GetPluginInfo(processData.pluginUsed, properties) == IMCodec::ImageResult::Success)
            pluginDescription = properties.pluginDescription;

        messageValues.emplace_back("Codec used", MessageFormatter::ValueObjectList{ {       pluginDescription   } });


        auto uniqueValues = rasterized->GetNumUniqueColors();
        if (uniqueValues > -1)
            messageValues.emplace_back("Unique values", MessageFormatter::ValueObjectList{ {uniqueValues } });


        // Add meta data
        const auto metaDataPtr = oivImage->GetMetaData();
        if (metaDataPtr != nullptr)
        {
            auto metaData = *metaDataPtr;
            if (metaData.exifData.longitude != std::numeric_limits<double>::max())
                messageValues.emplace_back("Longitude", MessageFormatter::ValueObjectList{ { { metaData.exifData.longitude } , {6} } });

            if (metaData.exifData.latitude != std::numeric_limits<double>::max())
                messageValues.emplace_back("Latitude", MessageFormatter::ValueObjectList{ { {metaData.exifData.latitude} , {6} } });

            if (metaData.exifData.altitude != std::numeric_limits<double>::max())
                messageValues.emplace_back("Altitude", MessageFormatter::ValueObjectList{ {metaData.exifData.altitude},{"m"} });

            if (metaData.exifData.make.empty() == false)
                messageValues.emplace_back("Manufacturer", MessageFormatter::ValueObjectList{ {metaData.exifData.make } });

            if (metaData.exifData.model.empty() == false)
                messageValues.emplace_back("Model", MessageFormatter::ValueObjectList{ {metaData.exifData.model } });

            if (metaData.exifData.software.empty() == false)
                messageValues.emplace_back("Software", MessageFormatter::ValueObjectList{ {metaData.exifData.software} });

            if (metaData.exifData.copyright.empty() == false)
                messageValues.emplace_back("Copyright", MessageFormatter::ValueObjectList{ { metaData.exifData.copyright } });

        }
        message += L'\n' + MessageFormatter::FormatMetaText(args);
        return message;
    }
}