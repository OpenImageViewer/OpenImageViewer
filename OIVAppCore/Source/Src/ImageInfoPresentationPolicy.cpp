#include <LLUtils/StringDefs.h>
#include <OIVAppCore/ImageInfoPresentationPolicy.h>

#include <OIVShared/UnitFormatter.h>

#include <IImageCodec.h>
#include <Image.h>
#include <LLUtils/Exception.h>
#include <LLUtils/StringUtility.h>
#include <OIVImage/OIVFileImage.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <limits>
#include <sstream>
#include <utility>

namespace OIV
{
    namespace
    {
        LLUtils::native_string_type FormatNumber(long double value, int precision = 2)
        {
            LLUtils::native_stringstream stream;
            stream << std::fixed << std::setprecision(precision) << value;
            return stream.str();
        }

        void AddRow(ImageInfoPresentationPolicy::ImageInfoRows& rows, const std::string& key,
                    std::initializer_list<LLUtils::native_string_type> values)
        {
            ImageInfoPresentationPolicy::ImageInfoRow row;
            row.key = key;
            row.values.reserve(values.size());
            for (const LLUtils::native_string_type& value : values)
                row.values.push_back({value});

            rows.push_back(std::move(row));
        }

        const char* FormatSemantic(IMCodec::ChannelSemantic semantic)
        {
            switch (semantic)
            {
                case IMCodec::ChannelSemantic::Red:
                    return "R";
                case IMCodec::ChannelSemantic::Green:
                    return "G";
                case IMCodec::ChannelSemantic::Blue:
                    return "B";
                case IMCodec::ChannelSemantic::Opacity:
                    return "A";
                case IMCodec::ChannelSemantic::Monochrome:
                    return "Monochrome";
                case IMCodec::ChannelSemantic::Float:
                    return "Float";
                case IMCodec::ChannelSemantic::None:
                default:
                    return "Undefined";
            }
        }

        const char* FormatDataType(IMCodec::ChannelDataType dataType)
        {
            switch (dataType)
            {
                case IMCodec::ChannelDataType::Float:
                    return "float";
                case IMCodec::ChannelDataType::SignedInt:
                    return "signed";
                case IMCodec::ChannelDataType::UnsignedInt:
                    return "unsigned";
                case IMCodec::ChannelDataType::None:
                default:
                    return "undefined";
            }
        }
    }  // namespace

    ImageInfoPresentationPolicy::ImageInfoRows ImageInfoPresentationPolicy::Build(
        const OIVBaseImageSharedPtr& image, const OIVBaseImageSharedPtr& rasterized, IMCodec::IImageCodec& imageCodec)
    {
        ImageInfoRows rows;
        std::shared_ptr<OIVFileImage> fileImage = std::dynamic_pointer_cast<OIVFileImage>(image);

        if (fileImage != nullptr)
        {
            const std::filesystem::path filePath = fileImage->GetFileName();
            AddRow(rows, "File path", {filePath.native()});

            const uintmax_t fileSize = std::filesystem::file_size(filePath);
            AddRow(rows, "File size", {UnitFormatter::FormatUnit(fileSize, UnitType::BinaryDataShort, 0, 0)});
            AddRow(rows, "File date", {FormatFileTime(filePath)});

            const uint32_t bitmapSize          = rasterized->GetImage()->GetTotalSizeOfImageTexels();
            const long double compressionRatio = fileSize == 0 ? 0.0L
                                                               : static_cast<long double>(bitmapSize) /
                                                                     static_cast<long double>(fileSize);
            AddRow(rows, "Compression ratio", {LLUTILS_TEXT("1:"), FormatNumber(compressionRatio)});
        }
        else if (image != nullptr)
        {
            AddRow(rows, "Source", {FormatImageSource(image->GetImageSource())});
        }

        const bool isAnimation = rasterized->GetImage()->GetItemType() == IMCodec::ImageItemType::Container &&
                                 rasterized->GetImage()->GetSubImageGroupType() ==
                                     IMCodec::ImageItemType::AnimationFrame;

        if (isAnimation)
            AddRow(rows, "Num frames",
                   {LLUtils::StringUtility::ToNativeString(std::to_string(rasterized->GetImage()->GetNumSubImages()))});
        else
            AddRow(rows, "Num sub-images",
                   {LLUtils::StringUtility::ToNativeString(std::to_string(rasterized->GetImage()->GetNumSubImages()))});

        IMCodec::ImageSharedPtr frame = isAnimation ? rasterized->GetImage()->GetSubImage(0) : rasterized->GetImage();

        AddRow(rows, "Width",
               {LLUtils::StringUtility::ToNativeString(std::to_string(frame->GetWidth())), LLUTILS_TEXT("px")});
        AddRow(rows, "Height",
               {LLUtils::StringUtility::ToNativeString(std::to_string(frame->GetHeight())), LLUTILS_TEXT("px")});
        AddRow(rows, "bit depth",
               {LLUtils::StringUtility::ToNativeString(std::to_string(frame->GetBitsPerTexel())),
                LLUTILS_TEXT(" bpp")});
        AddRow(rows, "channels info", {FormatTexelInfo(frame->GetTexelInfo())});
        if (frame->GetOriginalTexelFormat() != IMCodec::TexelFormat::UNKNOWN &&
            frame->GetOriginalTexelFormat() != frame->GetTexelFormat())
        {
            AddRow(rows, "original channels info", {FormatTexelInfo(frame->GetOriginalTexelInfo())});
        }

        if (image != nullptr && image->GetImage() != nullptr)
        {
            const IMCodec::ItemProcessData& processData = image->GetImage()->GetProcessData();
            AddRow(rows, "Load time",
                   {FormatNumber(static_cast<long double>(processData.processTime)), LLUTILS_TEXT("ms")});
            AddRow(rows, "Display time",
                   {FormatNumber(static_cast<long double>(rasterized->GetDisplayTime())), LLUTILS_TEXT("ms")});

            LLUtils::native_string_type pluginDescription = LLUTILS_TEXT("Unknown");
            IMCodec::PluginProperties properties;
            if (imageCodec.GetPluginInfo(processData.pluginUsed, properties) == IMCodec::ImageResult::Success)
                pluginDescription = LLUtils::StringUtility::ConvertString<LLUtils::native_string_type>(
                    properties.pluginDescription);

            AddRow(rows, "Codec used", {pluginDescription});
        }

        const int64_t uniqueValues = rasterized->GetNumUniqueColors();
        if (uniqueValues > -1)
            AddRow(rows, "Unique values", {LLUtils::StringUtility::ToNativeString(std::to_string(uniqueValues))});

        if (image != nullptr && image->GetMetaData() != nullptr)
        {
            const IMCodec::ItemMetaDataSharedPtr& metaData = image->GetMetaData();
            const IMCodec::ExifData& exifData              = metaData->exifData;
            if (exifData.longitude != std::numeric_limits<double>::max())
                AddRow(rows, "Longitude", {FormatNumber(exifData.longitude, 6)});

            if (exifData.latitude != std::numeric_limits<double>::max())
                AddRow(rows, "Latitude", {FormatNumber(exifData.latitude, 6)});

            if (exifData.altitude != std::numeric_limits<double>::max())
                AddRow(rows, "Altitude", {FormatNumber(exifData.altitude), LLUTILS_TEXT("m")});

            if (exifData.make.empty() == false)
                AddRow(rows, "Manufacturer", {LLUtils::StringUtility::ToNativeString(exifData.make)});

            if (exifData.model.empty() == false)
                AddRow(rows, "Model", {LLUtils::StringUtility::ToNativeString(exifData.model)});

            if (exifData.software.empty() == false)
                AddRow(rows, "Software", {LLUtils::StringUtility::ToNativeString(exifData.software)});

            if (exifData.copyright.empty() == false)
                AddRow(rows, "Copyright", {LLUtils::StringUtility::ToNativeString(exifData.copyright)});
        }

        return rows;
    }

    LLUtils::native_string_type ImageInfoPresentationPolicy::FormatFileTime(const std::filesystem::path& filePath)
    {
        const std::filesystem::file_time_type fileTime = std::filesystem::last_write_time(filePath);
        const auto systemTime = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            fileTime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
        const std::time_t time = std::chrono::system_clock::to_time_t(systemTime);

        std::tm localTime{};
#ifdef _WIN32
        errno_t errorCode = localtime_s(&localTime, &time);
        if (errorCode != 0)
        {
            using namespace std::string_literals;
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState,
                         "could not convert time_t to a tm structure, error code: "s + std::to_string(errorCode));
        }
#else
        if (localtime_r(&time, &localTime) == nullptr)
            LL_EXCEPTION(LLUtils::Exception::ErrorCode::InvalidState, "could not convert time_t to a tm structure");
#endif

        LLUtils::native_stringstream stream;
        stream << std::put_time(&localTime, LLUTILS_TEXT("%Y-%m-%d %X"));
        return stream.str();
    }

    LLUtils::native_string_type ImageInfoPresentationPolicy::FormatImageSource(ImageSource source)
    {
        switch (source)
        {
            case ImageSource::None:
                return LLUTILS_TEXT("none");
            case ImageSource::File:
                return LLUTILS_TEXT("file");
            case ImageSource::Clipboard:
                return LLUTILS_TEXT("clipboard");
            case ImageSource::ClipboardText:
                return LLUTILS_TEXT("clipboard text");
            case ImageSource::InternalText:
                return LLUTILS_TEXT("internal text");
            case ImageSource::GeneratedByLib:
                return LLUTILS_TEXT("auto generated");
            default:
                return LLUTILS_TEXT("unknown");
        }
    }

    LLUtils::native_string_type ImageInfoPresentationPolicy::FormatTexelInfo(const IMCodec::TexelInfo& texelInfo)
    {
        if (texelInfo.numChannles == 0)
            return {};

        bool sameDataTypeForAllChannels   = true;
        IMCodec::ChannelDataType dataType = texelInfo.channles[0].channelDataType;
        for (size_t i = 1; i < texelInfo.numChannles; i++)
        {
            if (texelInfo.channles[i].channelDataType != dataType)
            {
                sameDataTypeForAllChannels = false;
                break;
            }
        }

        LLUtils::native_stringstream stream;
        for (size_t i = 0; i < texelInfo.numChannles; i++)
        {
            const IMCodec::ChannelInfo& channel = texelInfo.channles[i];
            stream << LLUtils::StringUtility::ConvertString<LLUtils::native_string_type>(
                          std::string(FormatSemantic(channel.semantic)))
                   << LLUTILS_TEXT(':');
            if (sameDataTypeForAllChannels == false || channel.semantic == IMCodec::ChannelSemantic::Monochrome)
                stream << LLUTILS_TEXT('(')
                       << LLUtils::StringUtility::ConvertString<LLUtils::native_string_type>(
                              std::string(FormatDataType(channel.channelDataType)))
                       << LLUTILS_TEXT(')');

            stream << static_cast<int>(channel.width);
            if (i + 1 < texelInfo.numChannles)
                stream << LLUTILS_TEXT(' ');
        }

        return stream.str();
    }
}  // namespace OIV
