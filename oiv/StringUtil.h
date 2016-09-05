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
        
        int maxStrSize = wcslen(str) * 2 + 2;
        char* pBuff = new char[maxStrSize];
        wcstombs(pBuff, str, maxStrSize);
        std::string retData = pBuff;
        delete[] pBuff;
        return retData;


        /*static int ind = 0;
        static std::vector<std::string> cache(20);
        if (str == NULL)
        {
            return NULL;
        }
        else
        {
            ind = (ind + 1) % 20;
            cache[ind] = ToAString(std::wstring(str));
            return cache[ind].c_str();
        }*/

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
