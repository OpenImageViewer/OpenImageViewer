#pragma once
#include <string>
class StringUtility
{
public:
    static const char* ToAString(const char* str)
    {
        return str;
    }
    static std::string ToAString(const wchar_t* str)
    {
        
        size_t strLength = wcslen(str) + 1;


        char* pBuff = new char[strLength];
        size_t converted;
        wcstombs_s(&converted, pBuff, strLength, str, strLength * 2);
        std::string retData = pBuff;
        delete[] pBuff;
        return retData;
    }
    static std::string ToAString(const std::string& str)
    {
        return str;
    }
    static std::string ToAString(const std::wstring& str)
    {
        return ToAString(str.c_str());
    }
};
