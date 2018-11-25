#pragma once
#include <type_traits>
#include <cmath>
#include "Platform.h"

namespace LLUtils
{
    class Math
    {
    public:
        static constexpr double PI = 3.14159265358979323846;
        template <typename T, typename std::enable_if_t<std::is_integral_v<T>, int> = 0>
        constexpr LLUTILS_FORCE_INLINE static T Modulu(T val, T mod)
        {
            return (mod + (val % mod)) % mod;
        }
        template <typename T, typename std::enable_if_t<std::is_floating_point_v<T>, int> = 0 >
        constexpr LLUTILS_FORCE_INLINE static T Modulu(T val, T mod)
        {
            return fmod(mod + fmod(val, mod), mod);
        }

        template <typename T>
        constexpr LLUTILS_FORCE_INLINE static T Sign(T val)
        {
            return (static_cast<T>(0) < val) - (val < static_cast<T>(0));
        }

        template <typename T>
        constexpr LLUTILS_FORCE_INLINE static T ToDegrees(T val)
        {
            return (val * 180) / PI;
        }

        template <typename T>
        constexpr LLUTILS_FORCE_INLINE static T ToRadians(T val)
        {
            return (val * PI) / 180;
        }

    };
}