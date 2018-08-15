#pragma once
#include <string>
#include <vector>
#include <sstream>

namespace LLUtils
{
    using native_char_type = wchar_t;
    using native_string_type = std::basic_string<native_char_type>;
    using native_stringstream = std::basic_stringstream<native_char_type>;

    using default_char_type = char16_t;
    using default_string_type = std::basic_string<default_char_type>;
    using default_stringstream_type = std::basic_stringstream<default_char_type>;

    template <class string_type = default_string_type>
    using ListString = std::vector<string_type>;
    using ListStringIterator = ListString<>::iterator;

    using ListAString = ListString<std::string>;
    using ListAStringIterator = ListAString::iterator;
    using ListWString = ListString<std::wstring>;
    using ListWStringIterator = ListWString::iterator;


}

  