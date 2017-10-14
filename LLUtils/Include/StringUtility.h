#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

namespace LLUtils
{
    
    using default_char_type = wchar_t;
    using default_string_type = std::basic_string<default_char_type>;
    using stringstream_type = std::basic_stringstream<default_char_type>;
    
    template <class string_type>
    using ListString = std::vector<string_type>;
    

    using ListAString = ListString<std::string>;
    using ListWString = ListString<std::wstring>;

   using  ListStringIterator = ListString<std::wstring>::iterator;

    /*template <class string_type = default_string_type, typename ListString<string_type>::iterator>
    using  ListStringIterator = ListString<string_type>::iterator;*/

    class StringUtility
    {
    public:

        template< class chartype = default_char_type, typename string_type = std::basic_string<chartype>>
        static ListString<string_type> split(const string_type &s, char delim)
        {
            using namespace std;
            ListString<string_type> elems;
            basic_stringstream<string_type::value_type> ss;
            ss.str(s);
            string_type item;
            while (getline<string_type::value_type>(ss, item, delim))
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

  
        template <class string_type>
        static string_type GetFileExtension(const string_type& str)
        {
            using namespace std;
            string_type extension;
            
            string_type::size_type pos = str.find_last_of(46);
            if (pos != string_type::npos)
                extension = str.substr(pos + 1, str.length() - pos - 1);

            return extension;
        }

        template <class string_type>
        static string_type ToLower(const string_type& str)
        {
            using namespace std;
            string_type localStr = str;
            std::transform(localStr.begin(), localStr.end(), localStr.begin(), ::tolower);
            return localStr;
        }
    };
}
