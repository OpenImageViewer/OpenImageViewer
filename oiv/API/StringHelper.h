#pragma once
#include "defs.h"
#include <StringUtility.h>
#ifdef __cplusplus

template <typename CHAR_TYPE>
    OIVString OIV_ToOIVString(const CHAR_TYPE* str)
    {
        using SourceString = std::basic_string<typename std::remove_const<CHAR_TYPE>::type>;
    //using nonConstCharType = std::remove_const<CHAR_TYPE>::type;
        OIVString d;
        LLUtils::StringUtility::ConvertString(SourceString(str), d);
        return d;
    }

    template <typename STRING_TYPE, typename CHAR_TYPE = typename STRING_TYPE::value_type>
    OIVString OIV_ToOIVString(const STRING_TYPE& str)
    {
        OIVString d;
        LLUtils::StringUtility::ConvertString(str, d);
        return d;
    }
#endif
