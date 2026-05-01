#pragma once
#include <string>
#include <map>
#include <variant>
#include <TexelFormat.h>
namespace OIV
{
    class MessageFormatter
    {
    public:
        static inline std::wstring DefaultKeyColor = L"<textcolor=#ff8930>";
        static inline std::wstring DefaultValueColor = L"<textcolor=#98f733>";
        static inline std::wstring DefaultHeaderColor = L"<textcolor=#ff00ff>";

        struct ValueFormatArgs
        {
            int precsion = 2;
        };

        using ValueObjectBase =  std::variant<int64_t, long double, std::string, std::wstring>;
        struct ValueObject
        {
            ValueObject(const ValueObjectBase& objectBase) : valueObject(objectBase)
            {

            }

            ValueObject(const ValueObjectBase& objectBase, const ValueFormatArgs& args) : valueObject(objectBase)
                , formatArgs(args)
            {

            }
            ValueObjectBase valueObject{};
            ValueFormatArgs formatArgs{};

        };

        using ValueObjectList = std::vector<ValueObject>;
        using MessagesValues = std::vector<std::pair<std::string, ValueObjectList>>;
        
        struct FormatArgs
        {
            MessagesValues messageValues;
            std::wstring keyColor = DefaultKeyColor;
            std::wstring valueColor = DefaultValueColor;
            uint16_t maxLines = 24;
            uint16_t minSpaceFromValue = 3;
            uint16_t doubleWidth = 2;
            uint16_t spaceBetweenColumns = 3;
            char columnsSeperator = '|';
            char spacer = '.';
        };

        struct DecomposedPath
        {
            std::wstring parentPath;
            std::wstring fileName;
            std::wstring extension;
        };

        static std::wstring FormatValueObject(const ValueObjectList& objects);
        static std::wstring FormatValueObject(const ValueObject& valueObject);
        static std::wstring FormatMetaText(FormatArgs args);
        static std::string FormatTexelInfo(const IMCodec::TexelInfo& texelInfo);
        static const char* FormatSemantic(IMCodec::ChannelSemantic semantic);
        static const std::string& PickColor(IMCodec::ChannelSemantic semantic);
        static const char* FormatDataType(IMCodec::ChannelDataType dataType);
        static std::wstring FormatFilePath(const std::filesystem::path& filePath);
        static DecomposedPath DecomposePath(const std::filesystem::path& filePath);

        template<typename string_type, typename number_type>
        static string_type numberFormatWithCommas(number_type value, const ValueFormatArgs& format);
    };
}