#include <OIVAppCore/OIVHelper.h>

#include <OIVAppCore/MessageFormatter.h>

#include <ExoticNumbers/Float24.h>
#include <ExoticNumbers/half.hpp>
#include <ExoticNumbers/Int24.h>
#include <LLUtils/Exception.h>

#include <climits>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>

namespace OIV
{
    const std::string& OIVHelper::PickColor(IMCodec::ChannelSemantic semantic)
    {
        // Preserve this API's fallback for unspecified or unknown semantics while sharing the palette.
        static const std::string other = "<textcolor=#ff8930>";
        return semantic >= IMCodec::ChannelSemantic::Red && semantic <= IMCodec::ChannelSemantic::Black
                   ? MessageFormatter::PickColor(semantic)
                   : other;
    }

    std::string OIVHelper::ParseTexelValue(IMCodec::ImageSharedPtr image, LLUtils::PointI32 pixelPos)
    {
        using namespace IMCodec;
        std::stringstream stream;
        const auto* buffer = reinterpret_cast<const uint8_t*>(image->GetBufferAt(pixelPos.x, pixelPos.y));
        const auto& info   = image->GetTexelInfo();
        int currentPos     = 0;

        for (size_t i = 0; i < info.numChannles; i++)
        {
            const auto& channel = info.channles.at(i);
            if (channel.semantic != ChannelSemantic::None)
            {
                switch (channel.channelDataType)
                {
                    case ChannelDataType::UnsignedInt:
                        stream << PickColor(channel.semantic) << MessageFormatter::FormatSemantic(channel.semantic);
                        break;
                    case ChannelDataType::SignedInt:
                        stream << PickColor(channel.semantic) << "(signed)"
                               << MessageFormatter::FormatSemantic(channel.semantic);
                        break;
                    case ChannelDataType::Float:
                        stream << PickColor(channel.semantic) << MessageFormatter::FormatSemantic(channel.semantic);
                        break;
                    case ChannelDataType::None:
                        LL_EXCEPTION_UNEXPECTED_VALUE;
                }

                if (channel.width != 8 || channel.semantic == ChannelSemantic::Monochrome ||
                    channel.semantic == ChannelSemantic::Float)
                    stream << '(' << static_cast<int>(channel.width) << ')';

                stream << ':';

                constexpr int precision = 6;
                if (channel.channelDataType == ChannelDataType::Float)
                    stream << std::setprecision(precision) << std::setw(precision + 4) << std::setfill(' ')
                           << std::fixed;

                switch (channel.width)
                {
                    case 5:
                    case 6:
                        if (channel.channelDataType == ChannelDataType::UnsignedInt && info.texelSize == 16)
                        {
                            const uint16_t channelMask     = ((1 << channel.width) - 1) << currentPos;
                            const uint16_t wholeTexelValue = *reinterpret_cast<const uint16_t*>(buffer);
                            const uint8_t channelValue     = (wholeTexelValue & channelMask) >> currentPos;
                            stream << std::setw(2) << static_cast<int>(channelValue);
                        }
                        break;
                    case 8:
                        if (channel.channelDataType == ChannelDataType::UnsignedInt)
                            stream << std::setw(std::numeric_limits<uint8_t>::digits10 + 1)
                                   << static_cast<int>(
                                          *reinterpret_cast<const uint8_t*>(buffer + currentPos / CHAR_BIT));
                        else if (channel.channelDataType == ChannelDataType::SignedInt)
                            stream << std::setw(std::numeric_limits<uint8_t>::digits10 + 1)
                                   << static_cast<int>(
                                          *reinterpret_cast<const int8_t*>(buffer + currentPos / CHAR_BIT));
                        break;
                    case 16:
                        if (channel.channelDataType == ChannelDataType::UnsignedInt)
                            stream << std::setw(std::numeric_limits<uint16_t>::digits10 + 1)
                                   << *reinterpret_cast<const uint16_t*>(buffer + currentPos / CHAR_BIT);
                        else if (channel.channelDataType == ChannelDataType::SignedInt)
                            stream << std::setw(std::numeric_limits<int16_t>::digits10 + 1)
                                   << *reinterpret_cast<const int16_t*>(buffer + currentPos / CHAR_BIT);
                        else if (channel.channelDataType == ChannelDataType::Float)
                            stream << *reinterpret_cast<const half_float::half*>(buffer + currentPos / CHAR_BIT);
                        break;
                    case 24:
                        if (channel.channelDataType == ChannelDataType::Float)
                            stream << *reinterpret_cast<const Float24*>(buffer + currentPos / CHAR_BIT);
                        else if (channel.channelDataType == ChannelDataType::UnsignedInt)
                            stream << "N/A";
                        else if (channel.channelDataType == ChannelDataType::SignedInt)
                            stream << std::setw(8)
                                   << static_cast<int>(*reinterpret_cast<const Int24*>(buffer + currentPos / CHAR_BIT));
                        break;
                    case 32:
                        if (channel.channelDataType == ChannelDataType::Float)
                            stream << *reinterpret_cast<const float*>(buffer + currentPos / CHAR_BIT);
                        else if (channel.channelDataType == ChannelDataType::SignedInt)
                            stream << std::setw(std::numeric_limits<uint32_t>::digits10 + 1)
                                   << *reinterpret_cast<const uint32_t*>(buffer + currentPos / CHAR_BIT);
                        break;
                    case 64:
                        if (channel.channelDataType == ChannelDataType::UnsignedInt)
                            stream << std::setw(std::numeric_limits<uint64_t>::digits10 + 1)
                                   << *reinterpret_cast<const int64_t*>(buffer + currentPos / CHAR_BIT);
                        break;
                }

                stream << ' ';
            }
            currentPos += channel.width;
        }

        std::string message = stream.str();
        if (message.empty() == false)
            message.pop_back();

        return message;
    }
}  // namespace OIV
