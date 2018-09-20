#pragma once
#include <filesystem>

namespace LLUtils
{

    class Utility
    {
    public:
        template <class T>
        static T Align(T num ,T alignement)
        {
            static_assert(std::is_integral<T>(),"Alignment works only with integrals");
            if (alignement < 1)
                throw std::logic_error("alignement must be a positive value");
            return (num + (alignement - 1)) / alignement * alignement;
        }
        

        template <typename T>
        static T Sign(T val)
        {
            return (T(0) < val) - (val < T(0));
        }
    };
}