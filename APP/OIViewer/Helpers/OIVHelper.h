#pragma once
#include <string>
#include <API\defs.h>
#include <sstream>
#include <../ImageUtil/Include/half.hpp>
#include <../ImageUtil/Include/Float24.h>


class OIVHelper
{
public:
    static std::wstring ParseTexelValue(const OIV_CMD_TexelInfo_Response& texelInfo)
    {
        std::wstringstream ss;
        const uint8_t* const buffer = texelInfo.buffer;
        std::wstring defaultColor = L"<textcolor=#ff8930>";
        

        std::wstring blue  = L"<textcolor=#006dff>";
        std::wstring green = L"<textcolor=#00ff00>";
        std::wstring red   = L"<textcolor=#ff1c21>";
        std::wstring white = L"<textcolor=#ffffff>";

        ss << defaultColor;

        switch (texelInfo.type)
        {
        case OIV_TexelFormat::TF_I_B8_G8_R8_A8:
            ss  << "BGRA:" << std::setfill(L'0')
                << blue  << " " << std::setw(3) << buffer[0]
                << green << " " << std::setw(3) << buffer[1]
                << red   << " " << std::setw(3) << buffer[2]
                << white << " " << std::setw(3) << buffer[3];
            break;
        case OIV_TexelFormat::TF_I_B8_G8_R8:
            ss << "BGR:" << std::setfill(L'0')
                << blue << " " << std::setw(3) << buffer[0]
                << green << " " << std::setw(3) << buffer[1]
                << red << " " << std::setw(3) << buffer[2];
            break;

        case OIV_TexelFormat::TF_I_R8_G8_B8_A8:
            ss  << "RGBA:" << std::setfill(L'0')
                << red   << " " << std::setw(3) << buffer[0]
                << green << " " << std::setw(3) << buffer[1]
                << blue  << " " << std::setw(3) << buffer[2]
                << white << " " << std::setw(3) << buffer[3];
            break;
        case OIV_TexelFormat::TF_I_R8_G8_B8:
            ss << "RGB:" << std::setfill(L'0')
                << red << " " << std::setw(3) << buffer[0]
                << green << " " << std::setw(3) << buffer[1]
                << blue << " " << std::setw(3) << buffer[2];
            break;
        case OIV_TexelFormat::TF_F_X32:
            ss << "32 bit float: " << std::setw(10) << std::setfill(L'0') << std::fixed << std::setprecision(6) << *reinterpret_cast<const float*>(buffer);
            break;
        case OIV_TexelFormat::TF_F_X24:
            ss << "24 bit float: " << std::setw(10) << std::setfill(L'0') << std::fixed << std::setprecision(6) << static_cast<const float>(*reinterpret_cast<const Float24*>(buffer));
            break;
        case OIV_TexelFormat::TF_F_X16:
            ss << "16 bit float: " << std::setw(10) << std::setfill(L'0') << std::fixed << std::setprecision(6) << static_cast<const float>(*reinterpret_cast<const half_float::half*>(buffer));
            break;
        case OIV_TexelFormat::TF_I_X8:
            ss << "8 bit integer: " << std::setw(3) << std::setfill(L'0') << *reinterpret_cast<const uint8_t*>(buffer);
            break;
        default:
            ss << L"N/A";
            break;
        }
        return ss.str();
    }
};

