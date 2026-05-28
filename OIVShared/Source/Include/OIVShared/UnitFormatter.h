#pragma once

#include <LLUtils/StringDefs.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <string>

namespace OIV
{
    enum class UnitType
    {
        Undefined,
        TimeShort,
        BinaryDataShort,
        Distance,
        Frequency,
        BinaryBits,
        Count = BinaryBits
    };

    struct NumberTraits
    {
        uint32_t digits;
        uint64_t divider;
    };

    class UnitFormatter
    {
      public:

        template <typename T>
        static NumberTraits GetNumberTraits(T size, UnitType unitType)
        {
            if (unitType != UnitType::BinaryDataShort && unitType != UnitType::BinaryBits)
            {
                uint64_t log10OfSizeClampedToMultiplesOf3 =
                    size == 0
                        ? 0
                        : (static_cast<uint64_t>(std::log10(std::abs(static_cast<long double>(size)))) / 3ULL) * 3ULL;
                return {static_cast<uint32_t>(log10OfSizeClampedToMultiplesOf3),
                        static_cast<uint64_t>(std::pow(10.0L, log10OfSizeClampedToMultiplesOf3))};
            }
            else
            {
                uint64_t log2OfSizeDiv10ClampedToMultiplesOf3 =
                    size == 0
                        ? 0
                        : (static_cast<uint64_t>(std::log2(std::abs(static_cast<long double>(size)))) / 10ULL) * 3ULL;
                return {static_cast<uint32_t>(log2OfSizeDiv10ClampedToMultiplesOf3),
                        static_cast<uint64_t>(std::pow(1024.0, log2OfSizeDiv10ClampedToMultiplesOf3 / 3ULL))};
            }
        }

        template <typename T>
        static LLUtils::native_string_type FormatUnit(T size, UnitType type, int8_t precision, int8_t width)
        {
            constexpr size_t NumOfUnitScales = 6;
            constexpr size_t NumOfUnitTypes  = static_cast<size_t>(UnitType::Count);
            static constexpr std::array<std::array<const LLUtils::native_char_type*, NumOfUnitScales>, NumOfUnitTypes>
                unitsData = {{
                    {LLUTILS_TEXT(" sec"), LLUTILS_TEXT(" ms"), LLUTILS_TEXT(" us"), LLUTILS_TEXT(" ns"),
                     LLUTILS_TEXT(" ps"), LLUTILS_TEXT(" fs")},
                    {LLUTILS_TEXT(" bytes"), LLUTILS_TEXT("KB"), LLUTILS_TEXT("MB"), LLUTILS_TEXT("GB"),
                     LLUTILS_TEXT("TB"), LLUTILS_TEXT("PB")},
                    {LLUTILS_TEXT(" millimeters"), LLUTILS_TEXT(" meters"), LLUTILS_TEXT(" kilometers"),
                     LLUTILS_TEXT(" megametres"), LLUTILS_TEXT(" gigameters"), LLUTILS_TEXT(" terameters")},
                    {LLUTILS_TEXT(" Hz"), LLUTILS_TEXT(" Khz"), LLUTILS_TEXT(" Mhz"), LLUTILS_TEXT(" Ghz"),
                     LLUTILS_TEXT(" Thz"), LLUTILS_TEXT(" Phz")},
                    {LLUTILS_TEXT(" bits"), LLUTILS_TEXT("Kbits"), LLUTILS_TEXT("Mbits"), LLUTILS_TEXT("Gbits"),
                     LLUTILS_TEXT("Tbits"), LLUTILS_TEXT("Pbits")},
                }};

            NumberTraits traits = GetNumberTraits(size, type);
            thread_local static LLUtils::native_stringstream ss;
            ss.str(LLUtils::native_string_type());
            ss << std::fixed << std::setfill(LLUTILS_TEXT(' ')) << std::setw(width) << std::setprecision(precision)
               << static_cast<double>(size) / static_cast<double>(traits.divider)
               << unitsData[static_cast<size_t>(type) - 1][std::min<size_t>(NumOfUnitScales - 1, traits.digits / 3)];

            return ss.str();
        }
    };
}  // namespace OIV
