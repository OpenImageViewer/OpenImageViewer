#include "MessageFormatter.h"
namespace OIV
{
    std::string MessageFormatter::FormatValueObject(const ValueObjectList& objects)
    {
        std::string result;

        for (const auto& e : objects)
            result += FormatValueObject(e);

        return result;
    }

    std::string MessageFormatter::FormatValueObject(const ValueObject& valueObject)
    {
        switch (valueObject.index())
        {
        case 0:
            return numberFormatWithCommas(std::get<int64_t>(valueObject));
            break;
        case 1:
            return numberFormatWithCommas(std::get<long double>(valueObject));
            break;
        case 2:
            return std::get<std::string>(valueObject);
            break;
        }
    }

    std::string MessageFormatter::FormatMetaText(FormatArgs args)
    {
        using namespace std;

        struct ColumnInfo
        {
            size_t maxFirstLength;
            size_t maxSecondLength;
        };


        const int totalColumns = static_cast<int>(std::ceil(static_cast<double>(args.messageValues.size()) / args.maxLines));
        std::vector<ColumnInfo> columnInfo(totalColumns);
        int currentcolumn = 0;
        int currentLine = 0;

        for (const auto& [key, value] : args.messageValues)
        {
            auto formattedValue = FormatValueObject(value);

            columnInfo[currentcolumn].maxFirstLength = std::max(columnInfo[currentcolumn].maxFirstLength, key.length());
            columnInfo[currentcolumn].maxSecondLength = std::max(columnInfo[currentcolumn].maxSecondLength, formattedValue.length());
            currentLine++;
            if (currentLine >= args.maxLines)
            {
                currentLine = 0;
                currentcolumn++;
            }
        }

        currentcolumn = 0;
        currentLine = 0;



        vector<std::string> lines(std::min<int>(args.messageValues.size(), args.maxLines));


        for (const auto& [key, value] : args.messageValues)
        {
            stringstream ss;
            ss << args.keyColor << key;

            auto formattedValue = FormatValueObject(value);

            size_t currentLength = key.length();
            while (currentLength++ < columnInfo[currentcolumn].maxFirstLength)
                ss << args.spacer;

            for (int i = 0; i < args.minSpaceFromValue - 1; i++)
                ss << args.spacer;

            ss << ' ';

            ss << args.valueColor << formattedValue;

            if (currentcolumn < totalColumns - 1)
            {
                size_t currentLength = formattedValue.length();
                while (currentLength++ < columnInfo[currentcolumn].maxSecondLength)
                    ss << " ";


                auto halfColumSpace = args.spaceBetweenColumns / 2;

                for (int i = 0; i < halfColumSpace; i++)
                    ss << ' ';
                
                ss << "<textcolor=#444444>" << args.columnsSeperator;
                for (int i = halfColumSpace; i < args.spaceBetweenColumns - 1; i++)
                    ss << ' ';
            }

            lines[currentLine] += ss.str();

            currentLine++;
            if (currentLine >= args.maxLines)
            {
                currentLine = 0;
                currentcolumn++;
            }
        }

        stringstream ss1;
        for (const std::string& line : lines)
            ss1 << line << "\n";

        std::string message = ss1.str();
        if (message[message.size() - 1] == '\n')
            message.erase(message.size() - 1);


        return message;
    }

    std::string MessageFormatter::FormatTexelInfo(const IMCodec::TexelInfo& texelInfo)
    {
        std::stringstream ss;
        using namespace IMCodec;
        bool sameDataTypeForAllchannels = true;

        ChannelSemantic semantic = texelInfo.channles[0].semantic;
        for (size_t i = 1; i < texelInfo.numChannles; i++)
        {
            if (texelInfo.channles[i].semantic != semantic)
            {
                sameDataTypeForAllchannels = true;
                break;
            }
        }


        for (size_t i = 0; i < texelInfo.numChannles; i++)
        {
            switch (texelInfo.channles[i].semantic)
            {
            case IMCodec::ChannelSemantic::Red:
                ss << "R:";
                break;
            case IMCodec::ChannelSemantic::Green:
                ss << "G:";
                break;
            case IMCodec::ChannelSemantic::Blue:
                ss << "B:";
                break;
            case IMCodec::ChannelSemantic::Opacity:
                ss << "A:";
                break;
            case IMCodec::ChannelSemantic::Monochrome:
                ss << "Monochrome:";
                break;
            }
            if (sameDataTypeForAllchannels == false || texelInfo.channles[i].semantic == ChannelSemantic::Monochrome)
            {
                switch (texelInfo.channles[i].ChannelDataType)
                {
                case ChannelDataType::Float:
                    ss << "(float)";
                    break;
                case ChannelDataType::SignedInt:
                    ss << "(signed)";
                    break;
                case ChannelDataType::UnsignedInt:
                    ss << "(unsigned)";
                    break;
                }
            }

            ss << static_cast<int>(texelInfo.channles[i].width) << " ";
        }

        std::string str = ss.str();

        if (str.empty() == false)
            str.erase(str.size() - 1);

        return ss.str();
    }


    template<class T>
    static std::string MessageFormatter::numberFormatWithCommas(T value) {
        struct Numpunct : public std::numpunct<char> {
        protected:
            virtual char do_thousands_sep() const override { return ','; }
            virtual std::string do_grouping() const override { return "\03"; }
        };

        struct StringStreamWrapper
        {
            std::stringstream ss;
            StringStreamWrapper() { ss.imbue({ std::locale(), new Numpunct }); }
        } thread_local ssWrapper;
        auto& ss = ssWrapper.ss;

        ss.str(std::string());
        ss << std::setprecision(2) << std::fixed << value;
        return ss.str();
    }

}