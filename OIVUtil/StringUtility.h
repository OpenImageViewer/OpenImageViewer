#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
typedef std::vector<std::string> ListString;
class StringUtility
{
public:

    static ListString split(const std::string &s, char delim)
    {
        using namespace std;
        ListString elems;
        stringstream ss;
        ss.str(s);
        string item;
        while (getline(ss, item, delim))
            if (item.empty() == false)
                elems.push_back(item);

        return elems;
    }

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

    static std::string GetFileExtension(const std::string& str)
    {
        using namespace std;
        string extension;
        
        string::size_type pos = str.find_last_of(".");
        if (pos != std::string::npos)
            extension = str.substr(pos + 1, str.length() - pos - 1);

        return extension;
    }

    static std::wstring GetFileExtension(const std::wstring& str)
    {
        using namespace std;
        wstring extension;
        string::size_type pos = str.find_last_of(L".");
        if (pos != std::wstring::npos)
            extension = str.substr(pos + 1, str.length() - pos - 1);

        return extension;
    }

    static std::string ToLower(const std::string& str)
    {
        using namespace std;
        string localStr = str;
        std::transform(localStr.begin(), localStr.end(), localStr.begin(), ::tolower);
        return localStr;
    }
};
