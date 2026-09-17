#pragma once

#include <LLUtils/StringDefs.h>

#include <cmath>
#include <cstdint>

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

        enum class Precision
        {
            fixed,
            first_non_zero,
            predictive
        };

        struct FormatOptions
        {
            // Exact decimals for fixed mode; minimum decimals for adaptive modes.
            int8_t precision        = 2;
            int8_t width            = 0;
            Precision precisionMode = Precision::fixed;
            // Nonnegative step in the same units as size, before selecting a unit scale.
            double nextChange = 0.0;
        };

        template <typename T>
        static NumberTraits GetNumberTraits(T size, UnitType unitType)
        {
            // Values smaller than one retain the smallest unit without converting a negative
            // logarithm to an unsigned scale index.
            const auto magnitude = std::abs(static_cast<long double>(size));
            if (unitType != UnitType::BinaryDataShort && unitType != UnitType::BinaryBits)
            {
                uint64_t log10OfSizeClampedToMultiplesOf3 =
                    magnitude <= 1.0L ? 0 : (static_cast<uint64_t>(std::log10(magnitude)) / 3ULL) * 3ULL;
                return {static_cast<uint32_t>(log10OfSizeClampedToMultiplesOf3),
                        static_cast<uint64_t>(std::pow(10.0L, log10OfSizeClampedToMultiplesOf3))};
            }
            else
            {
                uint64_t log2OfSizeDiv10ClampedToMultiplesOf3 =
                    magnitude <= 1.0L ? 0 : (static_cast<uint64_t>(std::log2(magnitude)) / 10ULL) * 3ULL;
                return {static_cast<uint32_t>(log2OfSizeDiv10ClampedToMultiplesOf3),
                        static_cast<uint64_t>(std::pow(1024.0, log2OfSizeDiv10ClampedToMultiplesOf3 / 3ULL))};
            }
        }

        // Locale-independent fixed notation; Undefined formats a number without a unit suffix.
        // Adaptive modes extend precision until a nonzero value is visible; predictive also
        // distinguishes both next-step directions after rounding. A zero or unrepresentable
        // step falls back to first_non_zero. Supply finite size/step and finite scaled sums,
        // a nonnegative precision, and a valid unit type. Named units retain the uint64_t
        // magnitude range; Undefined supports the full finite double range.
        template <typename T>
        static LLUtils::native_string_type FormatUnit(T size, UnitType type, const FormatOptions& options)
        {
            const auto traits = type == UnitType::Undefined ? NumberTraits{0, 1} : GetNumberTraits(size, type);
            return FormatScaled(static_cast<double>(size), type, traits, options);
        }

      private:

        static LLUtils::native_string_type FormatScaled(double size, UnitType type, NumberTraits traits,
                                                        const FormatOptions& options);
    };
}  // namespace OIV
