#pragma once

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
                uint64_t log10OfSizeClampedToMultiplesOf3 = size == 0
                    ? 0
                    : (static_cast<uint64_t>(std::log10(std::abs(static_cast<long double>(size)))) / 3ULL) * 3ULL;
                return {static_cast<uint32_t>(log10OfSizeClampedToMultiplesOf3),
                        static_cast<uint64_t>(std::pow(10.0L, log10OfSizeClampedToMultiplesOf3))};
            }
            else
            {
                uint64_t log2OfSizeDiv10ClampedToMultiplesOf3 = size == 0
                    ? 0
                    : (static_cast<uint64_t>(std::log2(std::abs(static_cast<long double>(size)))) / 10ULL) * 3ULL;
                return {static_cast<uint32_t>(log2OfSizeDiv10ClampedToMultiplesOf3),
                        static_cast<uint64_t>(std::pow(1024.0, log2OfSizeDiv10ClampedToMultiplesOf3 / 3ULL))};
            }
        }

        template <typename T>
        static std::wstring FormatUnit(T size, UnitType type, int8_t precision, int8_t width)
        {
            constexpr size_t NumOfUnitScales = 6;
            constexpr size_t NumOfUnitTypes = static_cast<size_t>(UnitType::Count);
            static constexpr std::array<std::array<const wchar_t*, NumOfUnitScales>, NumOfUnitTypes> unitsData =
            {{
                {L" sec", L" ms", L" us", L" ns", L" ps", L" fs"},
                {L" bytes", L"KB", L"MB", L"GB", L"TB", L"PB"},
                {L" millimeters", L" meters", L" kilometers", L" megametres", L" gigameters", L" terameters"},
                {L" Hz", L" Khz", L" Mhz", L" Ghz", L" Thz", L" Phz"},
                {L" bits", L"Kbits", L"Mbits", L"Gbits", L"Tbits", L"Pbits"},
            }};

            NumberTraits traits = GetNumberTraits(size, type);
            thread_local static std::wstringstream ss;
            ss.str(std::wstring());
            ss << std::fixed << std::setfill(L' ') << std::setw(width) << std::setprecision(precision)
               << static_cast<double>(size) / static_cast<double>(traits.divider)
               << unitsData[static_cast<size_t>(type) - 1][std::min<size_t>(NumOfUnitScales - 1, traits.digits / 3)];

            return ss.str();
        }
    };
}  // namespace OIV
