#pragma once
#include <type_traits>
#include <cmath>
#include "Platform.h"

namespace LLUtils
{
    class Math
    {
    public:
        
        static constexpr double PI =  3.14159265358979323846;
        
		template <typename T, typename std::enable_if_t<std::is_integral_v<T>, int> = 0>
        static constexpr LLUTILS_FORCE_INLINE T Modulu(T val, T mod)
        {
            return (mod + (val % mod)) % mod;
        }
        template <typename T, typename std::enable_if_t<std::is_floating_point_v<T>, int> = 0>
        static constexpr LLUTILS_FORCE_INLINE T Modulu(T val, T mod)
        {
            return fmod(mod + fmod(val, mod), mod);
        }

        template <typename T, typename std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        static constexpr LLUTILS_FORCE_INLINE T Sign(T val)
        {
            return (static_cast<T>(0) < val) - (val < static_cast<T>(0));
        }

        template <typename T, typename std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        static constexpr LLUTILS_FORCE_INLINE T ToDegrees(T val)
        {
            return (val * static_cast<T>(180)) / static_cast<T>(PI);
        }

        template <typename T, typename std::enable_if_t<std::is_arithmetic_v<T>, int> = 0>
        static constexpr LLUTILS_FORCE_INLINE T ToRadians(T val)
        {
            return (val * static_cast<T>(PI)) / static_cast<T>(180);
        }
    };
}