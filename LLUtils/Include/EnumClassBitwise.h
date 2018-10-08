#ifndef LLUTILS_ENUM_FLAGS_H_
#define LLUTILS_ENUM_FLAGS_H_
    
    //unary ~operator
    template <class EnumType>
    inline EnumType& operator~ (EnumType& val)
    {
        val = static_cast<EnumType>(~static_cast<std::underlying_type_t<EnumType>>(val));
        return val;
    }


    // & operator
    template <class EnumType>
    inline EnumType operator& (EnumType lhs, EnumType rhs)
    {
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) & static_cast<std::underlying_type_t<EnumType>>(rhs));
    }

    
    // &= operator
    template <class EnumType>
    inline EnumType operator&= (EnumType& lhs, EnumType rhs)
    {
        lhs = static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) & static_cast<std::underlying_type_t<EnumType>>(rhs));
        return lhs;
    }

    //| operator
    template <class EnumType>
    inline EnumType operator| (EnumType lhs, EnumType rhs)
    {
        return static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) | static_cast<std::underlying_type_t<EnumType>>(rhs));
    }
    //|= operator
    template <class EnumType>
    inline EnumType& operator|= (EnumType& lhs, EnumType rhs)
    {
        lhs = static_cast<EnumType>(static_cast<std::underlying_type_t<EnumType>>(lhs) | static_cast<std::underlying_type_t<EnumType>>(rhs));
        return lhs;
    }

#endif// LLUTILS_ENUM_FLAGS_H_