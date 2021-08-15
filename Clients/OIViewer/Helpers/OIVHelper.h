#pragma once
#include <string>
#include <defs.h>
#include <sstream>
#include <limits>
#include "../ImageCodec/ImageLoader/Include/TexelFormat.h"
#include <ExoticNumbers/half.hpp>
#include <ExoticNumbers/Float24.h>
#include "MessageFormatter.h"
#include "ExoticNumbers/Int24.h"



namespace OIV
{
    class OIVHelper
    {
    public:

        static const std::string& PickColor(IMCodec::ChannelSemantic semantic)
        {
            using namespace IMCodec;
            const static std::string blue = "<textcolor=#006dff>";
            const static std::string green = "<textcolor=#00ff00>";
            const static std::string red = "<textcolor=#ff1c21>";
            const static std::string white = "<textcolor=#ffffff>";
            const static std::string other  = "<textcolor=#ff8930>";
            

            switch (semantic)
            {
            case ChannelSemantic::Red:
                return red;
            case ChannelSemantic::Green:
                return green;
            case ChannelSemantic::Blue:
                return blue;
            case ChannelSemantic::Opacity:
                return white;
            case ChannelSemantic::Monochrome:
            case ChannelSemantic::Float:
            case ChannelSemantic::None:
            default:

                return other;
            }
        }

        static std::string ParseTexelValue(const OIV_CMD_TexelInfo_Response& texelInfo)
        {
            using namespace IMCodec;
            std::stringstream ss;
            const uint8_t* const buffer = texelInfo.buffer;
            std::string defaultColor = "<textcolor=#ff8930>";

            //TODO: remove static cast after removing usage of C API 
            const auto& info = IMCodec::GetTexelInfo(static_cast<IMCodec::TexelFormat>(texelInfo.type));


            int currentpos = 0;

            for (size_t i = 0; i < info.numChannles; i++)
            {
                const auto& channel = info.channles.at(i);

                switch (channel.ChannelDataType)
                {
                case ChannelDataType::UnsignedInt:
                    ss << PickColor(channel.semantic) << MessageFormatter::FormatSemantic(channel.semantic);
                    break;
                case ChannelDataType::SignedInt:
                    ss << PickColor(channel.semantic) << "(signed)" << MessageFormatter::FormatSemantic(channel.semantic);
                    break;
                case ChannelDataType::Float:
                    ss << PickColor(channel.semantic) << MessageFormatter::FormatSemantic(channel.semantic);
                    break;
                case ChannelDataType::None:
                    LL_EXCEPTION_UNEXPECTED_VALUE;
                }

                if (channel.width != 8 
                    || channel.semantic ==  ChannelSemantic::Monochrome 
                    || channel.semantic == ChannelSemantic::Float)
                    ss << '(' << static_cast<int>(channel.width) << ')';

                ss << ':';


                //TODO: extract to user settings
                const int precision = 6;

                if (channel.ChannelDataType == ChannelDataType::Float)
                    ss << std::setprecision(precision) << std::setw(precision + 4) << std::setfill(' ') << std::fixed;
                

                switch (channel.width)
                {
                case 8:
                    if (channel.ChannelDataType == ChannelDataType::UnsignedInt)
                        ss << std::setw(std::numeric_limits<uint8_t>::digits10 + 1) << (int)*(reinterpret_cast<const uint8_t*> (buffer + (currentpos / CHAR_BIT)));
                    else if (channel.ChannelDataType == ChannelDataType::SignedInt)
                        ss << std::setw(std::numeric_limits<uint8_t>::digits10 + 1) << (int)*(reinterpret_cast<const int8_t*>(buffer + (currentpos / CHAR_BIT)));
                    break;
                case 16:
                    if (channel.ChannelDataType == ChannelDataType::UnsignedInt)
                        ss << std::setw(std::numeric_limits<uint16_t>::digits10 + 1) << *reinterpret_cast<const uint16_t*>((buffer + (currentpos / CHAR_BIT)));
                    else if (channel.ChannelDataType == ChannelDataType::SignedInt)
                        ss << std::setw(std::numeric_limits<int16_t>::digits10 + 1) << *reinterpret_cast<const int16_t*>((buffer + (currentpos / CHAR_BIT)));
                    else if (channel.ChannelDataType == ChannelDataType::Float)
                        ss <<  *reinterpret_cast<const half_float::half*>((buffer + (currentpos / CHAR_BIT)));
                    break;
                case 24:
                    if (channel.ChannelDataType == ChannelDataType::Float)
                        ss <<  *(reinterpret_cast<const Float24*>(buffer + (currentpos / CHAR_BIT)));
                    else if (channel.ChannelDataType == ChannelDataType::UnsignedInt)
                    {
                        ss << "N/A";
                        //TODO: implement.
                        //ss << std::setw(8) << *((reinterpret_cast<const*>(buffer) + currentpos / CHAR_BIT));
                    }
                        
                    else if (channel.ChannelDataType == ChannelDataType::SignedInt)
                        ss << std::setw(8) << (int)* (reinterpret_cast<const Int24*>(buffer + currentpos / CHAR_BIT));

                    break;
                case 32:
                    if (channel.ChannelDataType == ChannelDataType::Float)
                        ss << *(reinterpret_cast<const float*>(buffer + currentpos / CHAR_BIT));
                    else if (channel.ChannelDataType == ChannelDataType::SignedInt)
                        ss << std::setw(std::numeric_limits<uint32_t>::digits10 + 1) << *(reinterpret_cast<const uint32_t*>(buffer + (currentpos / CHAR_BIT)));
                    break;
                case 64:
                    if (channel.ChannelDataType == ChannelDataType::UnsignedInt)
                        ss << std::setw(std::numeric_limits<uint64_t>::digits10 + 1)  << *(reinterpret_cast<const int64_t*>(buffer + (currentpos / CHAR_BIT)));
                    break;
                    
                }
                
                ss << " ";
                currentpos += channel.width;

            }
            std::string msg = ss.str();
            if (msg.empty() == false)
                msg.erase(msg.length() - 1);
            return msg;
        }
    };

}