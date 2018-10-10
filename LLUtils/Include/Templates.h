#pragma once
namespace LLUtils
{
    class NoCopyable
    {
    public:
        NoCopyable() = default;
        NoCopyable(const NoCopyable&) = delete;
        NoCopyable& operator=(const NoCopyable&) = delete;
    };


template <typename T> 
T constexpr GetMaxBitsMask()
{
    // ((1 << (width of T - 1)) * 2 ) - 1
    return  (((static_cast<T>(1) << (sizeof(T) * static_cast<T>(8) - static_cast<T>(1))) - static_cast<T>(1)) << static_cast<T>(1)) + static_cast<T>(1);
}


}