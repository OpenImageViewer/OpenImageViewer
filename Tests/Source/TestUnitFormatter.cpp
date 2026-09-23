#include <catch2/catch_all.hpp>

#include <OIVShared/UnitFormatter.h>

#include <cmath>
#include <cstdint>
#include <limits>

static_assert(LLUtils::Math::Pow10<uint16_t>(3) == 1'000);
static_assert(LLUtils::Math::Pow10<uint64_t>(18) == 1'000'000'000'000'000'000ULL);
static_assert(LLUtils::Math::Pow2<int32_t>(10) == 1'024);
static_assert(LLUtils::Math::Pow2<uint64_t>(60) == 1ULL << 60);

TEST_CASE("Step-aware precision handles rounding carries and tiny finite values", "[Shared][UnitFormatter]")
{
    using Formatter                    = OIV::UnitFormatter;
    const auto [value, step, expected] = GENERATE(table<double, double, const LLUtils::native_char_type*>({
        {1.234, 0.01, LLUTILS_TEXT("1.23")},
        {1.234, 0.001, LLUTILS_TEXT("1.234")},
        {0.9999, 0.01, LLUTILS_TEXT("1.00")},
        {0.009, 0.00009, LLUTILS_TEXT("0.0090")},
        {0.00456, 0.0000456, LLUTILS_TEXT("0.00456")},
        {-0.009, 0.00009, LLUTILS_TEXT("-0.0090")},
        {0.0, 0.01, LLUTILS_TEXT("0.00")},
        {0.00000456, 0.0, LLUTILS_TEXT("0.000005")},
        {1.0, std::numeric_limits<double>::denorm_min(), LLUTILS_TEXT("1.00")},
    }));
    REQUIRE(Formatter::FormatUnit(value, OIV::UnitType::Undecorated,
                                  {
                                      .precisionMode = Formatter::PrecisionMode::step_aware,
                                      .stepSize      = step,
                                  }) == expected);

    const auto smallest = Formatter::FormatUnit(std::numeric_limits<double>::denorm_min(), OIV::UnitType::Undecorated,
                                                {
                                                    .precisionMode = Formatter::PrecisionMode::preserve_nonzero,
                                                });
    REQUIRE(smallest.size() == 326);
    REQUIRE(smallest.ends_with(LLUTILS_TEXT("5")));
    REQUIRE(Formatter::FormatUnit(std::numeric_limits<double>::max(), OIV::UnitType::Undecorated, {}).size() == 312);
    REQUIRE(Formatter::FormatUnit(0.0, OIV::UnitType::Undecorated, {}) == LLUTILS_TEXT("0.00"));
}

TEST_CASE("Unit formatting defaults to fixed precision and applies numeric padding", "[Shared][UnitFormatter]")
{
    using Formatter = OIV::UnitFormatter;
    REQUIRE(Formatter::FormatOptions{}.precisionMode == Formatter::PrecisionMode::fixed);
    REQUIRE(Formatter::FormatUnit(0.000456, OIV::UnitType::Undecorated, {}) == LLUTILS_TEXT("0.00"));
    REQUIRE(Formatter::FormatUnit(2048, OIV::UnitType::BinaryDataShort, {}) == LLUTILS_TEXT("2.00KB"));
    REQUIRE(Formatter::FormatUnit(2048, OIV::UnitType::BinaryDataShort,
                                  {
                                      .decimalPlaces = 1,
                                      .minimumWidth  = 7,
                                  }) == LLUTILS_TEXT("    2.0KB"));
    REQUIRE(Formatter::FormatUnit(1500, OIV::UnitType::Distance,
                                  {
                                      .minimumWidth = 6,
                                  }) == LLUTILS_TEXT("  1.50 meters"));
    REQUIRE(Formatter::FormatUnit(1500, OIV::UnitType::Distance,
                                  {
                                      .decimalPlaces = 2,
                                      .minimumWidth  = 6,
                                      .stepSize      = 0.0001,
                                  }) == LLUTILS_TEXT("  1.50 meters"));
    REQUIRE(Formatter::FormatUnit(1.75, OIV::UnitType::Undecorated,
                                  {
                                      .decimalPlaces = 0,
                                  }) == LLUTILS_TEXT("2"));
}

TEST_CASE("Adaptive precision uses the displayed unit scale and pads the final number", "[Shared][UnitFormatter]")
{
    using Formatter = OIV::UnitFormatter;
    REQUIRE(Formatter::FormatUnit(1500, OIV::UnitType::Distance,
                                  {
                                      .precisionMode = Formatter::PrecisionMode::step_aware,
                                      .stepSize      = 1.0,
                                  }) == LLUTILS_TEXT("1.500 meters"));
    REQUIRE(Formatter::FormatUnit(2048, OIV::UnitType::BinaryDataShort,
                                  {
                                      .precisionMode = Formatter::PrecisionMode::step_aware,
                                      .stepSize      = 1.0,
                                  }) == LLUTILS_TEXT("2.000KB"));
    REQUIRE(Formatter::FormatUnit(0.000456, OIV::UnitType::Distance,
                                  {
                                      .minimumWidth  = 8,
                                      .precisionMode = Formatter::PrecisionMode::preserve_nonzero,
                                  }) == LLUTILS_TEXT("  0.0005 millimeters"));
    REQUIRE(Formatter::FormatUnit(0.000456, OIV::UnitType::Undecorated,
                                  {
                                      .minimumWidth  = 10,
                                      .precisionMode = Formatter::PrecisionMode::step_aware,
                                      .stepSize      = 0.00000456,
                                  }) == LLUTILS_TEXT("  0.000456"));
    for (const auto mode : {Formatter::PrecisionMode::fixed, Formatter::PrecisionMode::preserve_nonzero,
                            Formatter::PrecisionMode::step_aware})
    {
        REQUIRE(Formatter::FormatUnit(1.25, OIV::UnitType::Undecorated,
                                      {
                                          .decimalPlaces = 5,
                                          .precisionMode = mode,
                                          .stepSize      = 0.01,
                                      }) == LLUTILS_TEXT("1.25000"));
    }
    REQUIRE(Formatter::FormatUnit(0.000456, OIV::UnitType::BinaryDataShort,
                                  {
                                      .precisionMode = Formatter::PrecisionMode::preserve_nonzero,
                                  }) == LLUTILS_TEXT("0.0005 bytes"));
}

TEST_CASE("Unit scale selection preserves threshold boundaries and negative values", "[Shared][UnitFormatter]")
{
    using Formatter                    = OIV::UnitFormatter;
    const auto [value, unit, expected] = GENERATE(table<double, OIV::UnitType, const LLUtils::native_char_type*>({
        {0.0, OIV::UnitType::Distance, LLUTILS_TEXT("0.00 millimeters")},
        {0.5, OIV::UnitType::Distance, LLUTILS_TEXT("0.50 millimeters")},
        {std::nextafter(1000.0, 0.0), OIV::UnitType::Distance, LLUTILS_TEXT("1000.00 millimeters")},
        {1000.0, OIV::UnitType::Distance, LLUTILS_TEXT("1.00 meters")},
        {std::nextafter(1024.0, 0.0), OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("1024.00 bytes")},
        {1024.0, OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("1.00KB")},
        {-1024.0, OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("-1.00KB")},
        {1'000'000.0, OIV::UnitType::Distance, LLUTILS_TEXT("1.00 kilometers")},
        {1'048'576.0, OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("1.00MB")},
        {1'048'576.0, OIV::UnitType::BinaryBits, LLUTILS_TEXT("1.00Mbits")},
        {1'000'000'000.0, OIV::UnitType::Distance, LLUTILS_TEXT("1.00 megametres")},
        {1'000'000'000'000.0, OIV::UnitType::Distance, LLUTILS_TEXT("1.00 gigameters")},
        {1'000'000'000'000'000.0, OIV::UnitType::Distance, LLUTILS_TEXT("1.00 terameters")},
        {static_cast<double>(1ULL << 30), OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("1.00GB")},
        {static_cast<double>(1ULL << 40), OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("1.00TB")},
        {static_cast<double>(1ULL << 50), OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("1.00PB")},
        {1000.0, OIV::UnitType::Undecorated, LLUTILS_TEXT("1000.00")},
        {-1'000'000.0, OIV::UnitType::Undecorated, LLUTILS_TEXT("-1000000.00")},
    }));
    REQUIRE(Formatter::FormatUnit(value, unit, {}) == expected);
}

TEST_CASE("Large values retain the divisor of the largest named unit", "[Shared][UnitFormatter]")
{
    using Formatter                    = OIV::UnitFormatter;
    const auto [value, unit, expected] = GENERATE(table<uint64_t, OIV::UnitType, const LLUtils::native_char_type*>({
        {1ULL << 60, OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("1024.00PB")},
        {1ULL << 60, OIV::UnitType::BinaryBits, LLUTILS_TEXT("1024.00Pbits")},
        {1'000'000'000'000'000'000ULL, OIV::UnitType::Distance, LLUTILS_TEXT("1000.00 terameters")},
        {1'000'000'000'000'000'000ULL, OIV::UnitType::Frequency, LLUTILS_TEXT("1000.00 Phz")},
        {std::numeric_limits<uint64_t>::max(), OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("16384.00PB")},
        {std::numeric_limits<uint64_t>::max(), OIV::UnitType::Distance, LLUTILS_TEXT("18446.74 terameters")},
    }));
    REQUIRE(Formatter::FormatUnit(value, unit, {}) == expected);
    REQUIRE(Formatter::FormatUnit(std::numeric_limits<int64_t>::min(), OIV::UnitType::BinaryDataShort, {}) ==
            LLUTILS_TEXT("-8192.00PB"));
}

TEST_CASE("Named unit types select their matching suffix rows", "[Shared][UnitFormatter]")
{
    const auto [unit, expected] = GENERATE(table<OIV::UnitType, const LLUtils::native_char_type*>({
        {OIV::UnitType::BinaryDataShort, LLUTILS_TEXT("1.00 bytes")},
        {OIV::UnitType::Distance, LLUTILS_TEXT("1.00 millimeters")},
        {OIV::UnitType::Frequency, LLUTILS_TEXT("1.00 Hz")},
        {OIV::UnitType::BinaryBits, LLUTILS_TEXT("1.00 bits")},
    }));
    REQUIRE(OIV::UnitFormatter::FormatUnit(1, unit, {}) == expected);
}
