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
        switch (texelInfo.type)
        {
        case OIV_TexelFormat::TF_I_B8_G8_R8_A8:
            ss  <<  "B:" << buffer[0]
                << " G:" << buffer[1]
                << " R:" << buffer[2]
                << " A:" << buffer[3];
            break;

        case OIV_TexelFormat::TF_I_R8_G8_B8_A8:
            ss  <<  "R:" << buffer[0]
                << " G:" << buffer[1]
                << " B:" << buffer[2]
                << " A:" << buffer[3];
            break;
        case OIV_TexelFormat::TF_F_X32:
            ss << "value: " << *reinterpret_cast<const float*>(buffer);
            break;
        case OIV_TexelFormat::TF_F_X24:
            ss << "value: " << static_cast<const float>(*reinterpret_cast<const Float24*>(buffer));
            break;
        case OIV_TexelFormat::TF_F_X16:
            ss << "value: " << static_cast<const float>(*reinterpret_cast<const half_float::half*>(buffer));
            break;
        case OIV_TexelFormat::TF_I_X8:
            ss << "value: " << *reinterpret_cast<const uint8_t*>(buffer);
            break;
        default:
            ss << L"N/A";
            break;
        }
        return ss.str();
    }
};

