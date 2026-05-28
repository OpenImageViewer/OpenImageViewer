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
        static inline LLUtils::native_string_type DefaultKeyColor = LLUTILS_TEXT("<textcolor=#ff8930>");
        static inline LLUtils::native_string_type DefaultValueColor = LLUTILS_TEXT("<textcolor=#98f733>");
        static inline LLUtils::native_string_type DefaultHeaderColor = LLUTILS_TEXT("<textcolor=#ff00ff>");

        struct ValueFormatArgs
        {
            int precsion = 2;
        };

        using ValueObjectBase =  std::variant<int64_t, long double, std::string, LLUtils::native_string_type>;
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
            LLUtils::native_string_type keyColor = DefaultKeyColor;
            LLUtils::native_string_type valueColor = DefaultValueColor;
            uint16_t maxLines = 24;
            uint16_t minSpaceFromValue = 3;
            uint16_t doubleWidth = 2;
            uint16_t spaceBetweenColumns = 3;
            char columnsSeperator = '|';
            char spacer = '.';
        };

        struct DecomposedPath
        {
            LLUtils::native_string_type parentPath;
            LLUtils::native_string_type fileName;
            LLUtils::native_string_type extension;
        };

        static LLUtils::native_string_type FormatValueObject(const ValueObjectList& objects);
        static LLUtils::native_string_type FormatValueObject(const ValueObject& valueObject);
        static LLUtils::native_string_type FormatMetaText(FormatArgs args);
        static std::string FormatTexelInfo(const IMCodec::TexelInfo& texelInfo);
        static const char* FormatSemantic(IMCodec::ChannelSemantic semantic);
        static const std::string& PickColor(IMCodec::ChannelSemantic semantic);
        static const char* FormatDataType(IMCodec::ChannelDataType dataType);
        static LLUtils::native_string_type FormatFilePath(const std::filesystem::path& filePath);
        static DecomposedPath DecomposePath(const std::filesystem::path& filePath);

        template<typename string_type, typename number_type>
        static string_type numberFormatWithCommas(number_type value, const ValueFormatArgs& format);
    };
}