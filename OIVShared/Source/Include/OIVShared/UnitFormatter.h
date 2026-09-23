#pragma once

#include <LLUtils/MathUtil.h>
#include <LLUtils/StringDefs.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string_view>

namespace OIV
{
    enum class UnitType
    {
        // Apply precision and padding without unit scaling or a suffix.
        Undecorated,
        BinaryDataShort,
        Distance,
        Frequency,
        BinaryBits,
        Count = BinaryBits
    };

    class UnitFormatter
    {
      public:

        enum class PrecisionMode
        {
            // Round to exactly decimalPlaces digits after the decimal point.
            fixed,
            // Use at least decimalPlaces, extending until a nonzero input no longer rounds to zero.
            preserve_nonzero,
            // Extend preserve_nonzero precision to distinguish both stepSize directions after rounding.
            // A zero or unrepresentable step falls back to preserve_nonzero.
            step_aware
        };

        struct FormatOptions
        {
            // Exact decimals for fixed mode; minimum decimals for adaptive modes.
            int8_t decimalPlaces = 2;
            // Minimum width of the numeric part, excluding any unit suffix.
            int8_t minimumWidth         = 0;
            PrecisionMode precisionMode = PrecisionMode::fixed;
            // Nonnegative step in the same units as size, before selecting a unit scale.
            double stepSize = 0.0;
        };

        // Locale-independent fixed notation. Supply finite size/stepSize and finite scaled sums,
        // nonnegative decimalPlaces, and a valid unit type. Named units retain the uint64_t
        // magnitude range, keeping the largest named unit for larger values within that range.
        // Undecorated supports the full finite double range.
        template <typename T>
        static LLUtils::native_string_type FormatUnit(T size, UnitType type, const FormatOptions& options)
        {
            return FormatScaled(static_cast<double>(size), type, GetMagnitudeInfo(size, type), options);
        }

      private:

        static constexpr size_t NumOfUnitScales = 6;

        struct MagnitudeInfo
        {
            uint32_t unit_magnitude_index;
            uint64_t divider;
        };

        static constexpr int MaxPrecision = std::numeric_limits<double>::max_digits10 -
                                            std::numeric_limits<double>::min_exponent10;
        // Sign, largest integer part, decimal point, and enough decimals for subnormal doubles.
        static constexpr size_t NumberBufferSize = std::numeric_limits<double>::max_exponent10 + MaxPrecision + 4;
        using NumberBuffer                       = std::array<char, NumberBufferSize>;

        struct DividerEntry
        {
            uint64_t decimalDivisor;
            uint64_t binaryDivisor;
        };

        template <typename T>
        static constexpr MagnitudeInfo GetMagnitudeInfo(T size, UnitType unitType)
        {
            // Undecorated values keep their original magnitude, even outside the named-unit range.
            MagnitudeInfo magnitudeInfo{
                .unit_magnitude_index = 0,
                .divider              = 1,
            };
            if (unitType != UnitType::Undecorated)
            {
                // Each row pairs the decimal and binary divisors for one unit magnitude.
                // Stop at the largest named unit so larger values retain a matching divisor and suffix.
                static constexpr std::array<DividerEntry, NumOfUnitScales> dividers{{
                    {LLUtils::Math::Pow10<uint64_t>(0), LLUtils::Math::Pow2<uint64_t>(0)},
                    {LLUtils::Math::Pow10<uint64_t>(3), LLUtils::Math::Pow2<uint64_t>(10)},
                    {LLUtils::Math::Pow10<uint64_t>(6), LLUtils::Math::Pow2<uint64_t>(20)},
                    {LLUtils::Math::Pow10<uint64_t>(9), LLUtils::Math::Pow2<uint64_t>(30)},
                    {LLUtils::Math::Pow10<uint64_t>(12), LLUtils::Math::Pow2<uint64_t>(40)},
                    {LLUtils::Math::Pow10<uint64_t>(15), LLUtils::Math::Pow2<uint64_t>(50)},
                }};

                const auto divisor = unitType == UnitType::BinaryDataShort || unitType == UnitType::BinaryBits
                                         ? &DividerEntry::binaryDivisor
                                         : &DividerEntry::decimalDivisor;
                // Convert before negating so the minimum signed integer remains representable.
                const auto value     = static_cast<long double>(size);
                const auto magnitude = value < 0.0L ? -value : value;
                const auto upper     = std::ranges::upper_bound(dividers, magnitude, {}, divisor);
                const auto index     = upper == dividers.begin() ? 0 : upper - dividers.begin() - 1;
                magnitudeInfo        = {
                    .unit_magnitude_index = static_cast<uint32_t>(index),
                    .divider              = dividers[index].*divisor,
                };
            }
            return magnitudeInfo;
        }

        static consteval bool ValidateMagnitudeInfo()
        {
            return GetMagnitudeInfo(999, UnitType::Distance).divider == 1 &&
                   GetMagnitudeInfo(1000, UnitType::Distance).divider == 1000 &&
                   GetMagnitudeInfo(1023, UnitType::BinaryDataShort).divider == 1 &&
                   GetMagnitudeInfo(1024, UnitType::BinaryDataShort).divider == 1024 &&
                   GetMagnitudeInfo(0, UnitType::Distance).unit_magnitude_index == 0 &&
                   GetMagnitudeInfo(1000, UnitType::Distance).unit_magnitude_index == 1 &&
                   GetMagnitudeInfo(1'000'000, UnitType::Distance).unit_magnitude_index == 2 &&
                   GetMagnitudeInfo(1ULL << 60, UnitType::BinaryDataShort).unit_magnitude_index == 5 &&
                   GetMagnitudeInfo(1ULL << 60, UnitType::BinaryDataShort).divider == (1ULL << 50) &&
                   GetMagnitudeInfo(1'000'000'000'000'000'000ULL, UnitType::Distance).divider ==
                       1'000'000'000'000'000ULL &&
                   GetMagnitudeInfo(std::numeric_limits<double>::max(), UnitType::Undecorated).divider == 1 &&
                   GetMagnitudeInfo(std::numeric_limits<double>::max(), UnitType::Undecorated).unit_magnitude_index ==
                       0;
        }

        static std::string_view FormatNumber(double value, double stepSize, const FormatOptions& options,
                                             NumberBuffer& output, NumberBuffer& scratch);
        static LLUtils::native_string_view GetUnitSuffix(UnitType type, uint32_t unitMagnitudeIndex);

        static LLUtils::native_string_type FormatScaled(double size, UnitType type, MagnitudeInfo traits,
                                                        const FormatOptions& options);
    };

    inline std::string_view UnitFormatter::FormatNumber(double value, double stepSize, const FormatOptions& options,
                                                        NumberBuffer& output, NumberBuffer& scratch)
    {
        const double up      = value + stepSize;
        const double down    = value - stepSize;
        const bool stepAware = options.precisionMode == PrecisionMode::step_aware && stepSize > 0.0 && up != value &&
                               down != value;

        // Adaptive modes begin near the first decimal that can carry meaningful information.
        // Starting one place earlier lets the actual conversion resolve rounding across a power-of-ten boundary.
        const double reference = stepAware && value != 0.0 ? std::min(stepSize, std::abs(value))
                                 : stepAware               ? stepSize
                                                           : std::abs(value);
        int digits             = options.decimalPlaces;
        if (options.precisionMode != PrecisionMode::fixed && reference > 0.0 && reference < 1.0)
            digits = std::max(digits, static_cast<int>(-std::floor(std::log10(reference))) - 1);

        // Convert into caller-owned buffers so repeated precision attempts and step comparisons allocate nothing.
        // preserve_nonzero stops once a nonzero input remains visible. step_aware additionally requires both
        // neighboring step values to produce text distinct from the current value.
        std::string_view number;
        bool sufficient = false;
        do
        {
            const auto result = std::to_chars(output.data(), output.data() + output.size(), value,
                                              std::chars_format::fixed, digits);
            number            = {output.data(), result.ptr};
            sufficient        = options.precisionMode == PrecisionMode::fixed || value == 0.0 ||
                                number.find_first_of("123456789") != std::string_view::npos;
            if (stepAware && sufficient)
            {
                const auto upResult = std::to_chars(scratch.data(), scratch.data() + scratch.size(), up,
                                                    std::chars_format::fixed, digits);
                sufficient          = number != std::string_view(scratch.data(), upResult.ptr);
                if (sufficient)
                {
                    const auto downResult = std::to_chars(scratch.data(), scratch.data() + scratch.size(), down,
                                                          std::chars_format::fixed, digits);
                    sufficient            = number != std::string_view(scratch.data(), downResult.ptr);
                }
            }
        } while (!sufficient && digits++ < MaxPrecision);

        return number;
    }

    inline LLUtils::native_string_view UnitFormatter::GetUnitSuffix(UnitType type, uint32_t unitMagnitudeIndex)
    {
        LLUtils::native_string_view suffix;
        if (type != UnitType::Undecorated)
        {
            constexpr size_t NumOfUnitTypes = static_cast<size_t>(UnitType::Count);
            // Views retain suffix lengths at compile time instead of measuring them per call.
            static constexpr std::array<std::array<LLUtils::native_string_view, NumOfUnitScales>, NumOfUnitTypes>
                unitsData = {{
                    {LLUTILS_TEXT(" bytes"), LLUTILS_TEXT("KB"), LLUTILS_TEXT("MB"), LLUTILS_TEXT("GB"),
                     LLUTILS_TEXT("TB"), LLUTILS_TEXT("PB")},
                    {LLUTILS_TEXT(" millimeters"), LLUTILS_TEXT(" meters"), LLUTILS_TEXT(" kilometers"),
                     LLUTILS_TEXT(" megametres"), LLUTILS_TEXT(" gigameters"), LLUTILS_TEXT(" terameters")},
                    {LLUTILS_TEXT(" Hz"), LLUTILS_TEXT(" Khz"), LLUTILS_TEXT(" Mhz"), LLUTILS_TEXT(" Ghz"),
                     LLUTILS_TEXT(" Thz"), LLUTILS_TEXT(" Phz")},
                    {LLUTILS_TEXT(" bits"), LLUTILS_TEXT("Kbits"), LLUTILS_TEXT("Mbits"), LLUTILS_TEXT("Gbits"),
                     LLUTILS_TEXT("Tbits"), LLUTILS_TEXT("Pbits")},
                }};
            // Undecorated occupies enum value zero but has no table row. Named types therefore use a one-based
            // enum-to-row offset. Scale selection already limits the index to the named-unit range.
            suffix = unitsData[static_cast<size_t>(type) - 1][unitMagnitudeIndex];
        }
        return suffix;
    }

    inline LLUtils::native_string_type UnitFormatter::FormatScaled(double size, UnitType type, MagnitudeInfo traits,
                                                                   const FormatOptions& options)
    {
        // Keep scale selection compile-time-testable even though formatting itself depends on runtime to_chars.
        static_assert(ValidateMagnitudeInfo());

        const double divisor = static_cast<double>(traits.divider);
        NumberBuffer output;
        NumberBuffer scratch;
        const auto number = FormatNumber(size / divisor, options.stepSize / divisor, options, output, scratch);
        const auto suffix = GetUnitSuffix(type, traits.unit_magnitude_index);

        // Padding applies only to the numeric field; the suffix follows immediately afterward.
        const auto padding = static_cast<size_t>(
            std::max(0, static_cast<int>(options.minimumWidth) - static_cast<int>(number.size())));
        LLUtils::native_string_type result;
        result.reserve(padding + number.size() + suffix.size());
        result.append(padding, LLUTILS_TEXT(' '));
        result.append(number.begin(), number.end());
        result.append(suffix);
        return result;
    }
}  // namespace OIV
