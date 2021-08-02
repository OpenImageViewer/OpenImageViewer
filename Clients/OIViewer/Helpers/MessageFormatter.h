#pragma once
#include <string>
#include <map>
#include <variant>
#include "../../External/ImageCodec/ImageLoader/Include/TexelFormat.h"
namespace OIV
{
    class MessageFormatter
    {
    public:
        static inline std::string DefaultKeyColor = "<textcolor=#ff8930>";
        static inline std::string DefaultValueColor = "<textcolor=#98f733>";
        static inline std::string DefaultHeaderColor = "<textcolor=#ff00ff>";

        using ValueObject =  std::variant<int64_t, long double, std::string>;
        using ValueObjectList = std::vector<ValueObject>;
        using MessagesValues = std::vector<std::pair<std::string, ValueObjectList>>;
        
        struct FormatArgs
        {
            MessagesValues messageValues;
            std::string keyColor = DefaultKeyColor;
            std::string valueColor = DefaultValueColor;
            uint16_t maxLines = 24;
            uint16_t minSpaceFromValue = 3;
            uint16_t doubleWidth = 2;
            uint16_t spaceBetweenColumns = 3;
            char columnsSeperator = '|';
            char spacer = '.';
        };

        static std::string FormatValueObject(const ValueObjectList& objects);
        static std::string FormatValueObject(const ValueObject& valueObject);
        static std::string FormatMetaText(FormatArgs args);
        static std::string FormatTexelInfo(const IMCodec::TexelInfo& texelInfo);

        template<class T>
        static std::string numberFormatWithCommas(T value);
    };
}