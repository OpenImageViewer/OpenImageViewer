#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>
#include <locale>
#include "StringDefs.h"

namespace LLUtils
{
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

        static inline default_string_type ToDefaultString(const native_string_type& str)
        {

            if (sizeof(default_string_type::value_type) != sizeof(native_string_type::value_type))
                throw std::logic_error("string conversion not implemented yet, native and default character type must be identical ");

            // a work around till the string convertions wil be fixed.
            return default_string_type(reinterpret_cast<const default_string_type::value_type*>(str.data()));


        }

        static inline native_string_type ToNativeString(const default_string_type& str)
        {
            if (sizeof(default_string_type::value_type) != sizeof(native_string_type::value_type))
                throw std::logic_error("string conversion not implemented yet, native and default character type must be identical ");

            return native_string_type(reinterpret_cast<const native_string_type::value_type*>(str.data()));
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
            
            
            typename string_type::size_type pos = str.find_last_of(46);
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

        template <class string_type>
        static string_type ToUpper(const string_type& str)
        {
            using namespace std;
            string_type localStr = str;
            std::transform(localStr.begin(), localStr.end(), localStr.begin(), ::toupper);
            return localStr;
        }

        template <typename SRC, typename SourceString = std::basic_string<SRC>, typename DestString = std::basic_string<char>>
        static DestString ToUTF8(const SourceString& source)
        {
            throw std::logic_error("not implemented");
            //DestString result;
            //std::wstring_convert<std::codecvt<SRC, char, std::mbstate_t>, SRC> convertor;
            //result = convertor.to_bytes(source);

            //return result;

        }

        template <typename SRC, typename SourceString = std::basic_string<SRC>, typename DestString = std::basic_string<char>>
        static void FromUTF8(const SourceString& source, DestString& result)
        {
            throw std::logic_error("not implemented");
            //std::wstring_convert<std::codecvt<SRC,char, std::mbstate_t>, SRC> convertor;
            //result = convertor.from_bytes(source);
        }

        template <typename SourceString, typename DestString, typename SRC = SourceString::value_type, typename DST = DestString::value_type>
        static void ConvertString(const SourceString& source, DestString& dest)
        {
        
            if (sizeof(SRC) != sizeof(DST))
                throw std::logic_error("string conversion not implemented yet, native and default character type must be identical ");

            dest = DestString(reinterpret_cast<const DestString::value_type*>(source.data()));
            

            //TODO: complete string conversions functions and uncomment the following two lines

            // std::string u8String = ToUTF8<SRC>(source);
             //FromUTF8<DST>(u8String, dest);
        }
    };
}
