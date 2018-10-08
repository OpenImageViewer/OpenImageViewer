#pragma once
#ifndef _BIT_FLAGS_H_
#define _BIT_FLAGS_H_
#include "EnumClassBitwise.h"
namespace LLUtils
{
    template<typename T>
    class BitFlags
    {
    public:
        constexpr inline BitFlags() = default;
        constexpr inline BitFlags(T value) { mValue = value; }
        constexpr inline BitFlags operator| (T rhs) const { return mValue | rhs; }
        constexpr inline BitFlags operator& (T rhs) const { return mValue & rhs; }
        constexpr inline BitFlags operator~ () const { return ~mValue; }
        constexpr inline operator T() const { return mValue; }
        constexpr inline BitFlags& operator|=(T rhs) { mValue |= rhs; return *this; }
        constexpr inline BitFlags& operator&=(T rhs) { mValue &= rhs; return *this; }
        constexpr inline bool test(T rhs) const { return (mValue & rhs) == rhs; }
        constexpr inline void set(T rhs) { mValue |= rhs; }
        constexpr inline void clear(T rhs) { mValue &= ~rhs; }

    private:
        T mValue;
    };
}
#endif //#define _BIT_FLAGS_H_
