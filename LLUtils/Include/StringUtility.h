#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace LLUtils
{/*
    template<class char_type>
    class CharDefs
    {
    public:
        using string_type = std::basic_string<char_type>;
        using stringstream_type = std::basic_stringstream<char_type> ;
        using ListString = std::vector<string_type> ;
        
    };*/
    
    typedef wchar_t  char_type;
    typedef std::basic_string<char_type> string_type;
    typedef std::basic_stringstream<char_type> stringstream_type;
    typedef std::vector<string_type> ListString;
    typedef ListString::iterator ListStringIterator;

    class StringUtility
    {
    public:

        //template< class chartype = wchar_t>
        static ListString split(const string_type &s, char delim)
        {
            using namespace std;
            ListString elems;
            
            stringstream_type ss;
            ss.str(s);
            string_type item;
            while (getline<char_type>(ss, item, delim))
                if (item.empty() == false)
                    elems.push_back(item);

            return elems;
        }

		static inline std::wstring ToWString( const std::string& str )
		{
            const std::size_t size = str.size() * 2 + 2;
			wchar_t* buf = new wchar_t[size];
			swprintf_s(buf, size, L"%S", str.c_str());
			std::wstring rval = buf;
			delete[] buf;
			return rval;
		}
        static const char* ToAString(const char* str)
        {
            return str;
        }
        static std::string ToAString(const wchar_t* str)
        {

            std::size_t strLength = wcslen(str) + 1;


            char* pBuff = new char[strLength];
            std::size_t converted;
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

        static string_type ToLower(const string_type& str)
        {
            using namespace std;
            string_type localStr = str;
            std::transform(localStr.begin(), localStr.end(), localStr.begin(), ::tolower);
            return localStr;
        }
    };
}