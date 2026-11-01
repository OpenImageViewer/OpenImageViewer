#include <catch2/catch_all.hpp>

#include <OIVAppCore/ViewActionController.h>
#include <OIVShared/AdaptiveMotion.h>

#include <array>
#include <cmath>

TEST_CASE("Zoom steps preserve acceleration across wheel and keyboard input", "[AppCore][Zoom]")
{
    double elapsedSeconds = 0.0;
    OIV::AdaptiveMotion previousMotion(1.0, 0.6, 1.0, [&]() { return elapsedSeconds; });
    OIV::AdaptiveMotion stepMotion(0.2, 0.6, 1.0, [&]() { return elapsedSeconds; });
    struct Input
    {
        double elapsed;
        double steps;
        double previousAmount;
    };
    const std::array inputs{
        Input{.elapsed = 0.0, .steps = 1.0, .previousAmount = 0.2},
        Input{.elapsed = 0.01, .steps = 1.0, .previousAmount = 0.2},
        Input{.elapsed = 0.01, .steps = 0.25, .previousAmount = 0.05},
        Input{.elapsed = 0.01, .steps = 0.5, .previousAmount = 0.1},
        Input{.elapsed = 0.01, .steps = -0.5, .previousAmount = -0.1},
        Input{.elapsed = 0.01, .steps = -1.0, .previousAmount = -0.2},
        Input{.elapsed = 0.01, .steps = -0.25, .previousAmount = -0.05},
        Input{.elapsed = 5.0, .steps = -1.0, .previousAmount = -0.2},
        Input{.elapsed = 0.01, .steps = 1.0, .previousAmount = 0.2},
    };
    for (const auto& input : inputs)
    {
        CAPTURE(input.elapsed, input.steps);
        elapsedSeconds = input.elapsed;
        REQUIRE(stepMotion.Add(input.steps) == Catch::Approx(previousMotion.Add(input.previousAmount)));
    }
}

TEST_CASE("Relative zoom snaps an approaching accelerated target to original size", "[AppCore][Zoom]")
{
    const auto [currentScale, amount] = GENERATE(table<double, double>({
        {0.95, 0.04},   // Approaches within the range from below.
        {1.05, -0.04},  // Approaches within the range from above.
        {0.98, 0.005},  // Already inside the range, moving toward 100%.
        {1.02, -0.005},
        {0.485, 1.0},  // Exactly 97%.
        {2.06, -1.0},  // Exactly 103%.
        {0.5, 1.0},    // Exactly 100%.
        {2.0, -1.0},
        {0.9, 0.5},  // Crosses 100%, landing beyond the range.
        {1.1, -0.5},
    }));
    CAPTURE(currentScale, amount);
    REQUIRE(OIV::ViewActionController::RelativeZoom(currentScale, amount) == 1.0);
}

TEST_CASE("Relative zoom does not snap outside the approach range", "[AppCore][Zoom]")
{
    const double belowRange = std::nextafter(0.97, 0.0);
    const double aboveRange = std::nextafter(1.03, 2.0);
    REQUIRE(OIV::ViewActionController::RelativeZoom(belowRange / 2.0, 1.0) == belowRange);
    REQUIRE(OIV::ViewActionController::RelativeZoom(aboveRange * 2.0, -1.0) == aboveRange);

    const auto [currentScale, amount, expectedScale] = GENERATE(table<double, double, double>({
        {0.98, -0.01, 0.98 / 1.01},  // Moving away while inside the range.
        {1.02, 0.01, 1.02 * 1.01},
        {1.0, 0.01, 1.01},  // Leaving 100% must not stick.
        {1.0, -0.01, 1.0 / 1.01},
        {0.98, 0.0, 0.98},  // No movement must not snap.
        {1.02, 0.0, 1.02},
        {1.0, 0.0, 1.0},
        {0.5, 0.1, 0.55},  // Normal zoom far from 100%.
        {2.0, -0.5, 2.0 / 1.5},
    }));
    CAPTURE(currentScale, amount);
    REQUIRE(OIV::ViewActionController::RelativeZoom(currentScale, amount) == Catch::Approx(expectedScale));
}

TEST_CASE("Zoom can leave a snap with its accumulated acceleration", "[AppCore][Zoom]")
{
    OIV::AdaptiveMotion motion(0.2, 0.6, 1.0, []() { return 0.0; });
    const double direction    = GENERATE(1.0, -1.0);
    const double initialScale = direction > 0.0 ? 0.96 : 1.04;
    const double firstAmount  = motion.Add(direction);
    const double snappedScale = OIV::ViewActionController::RelativeZoom(initialScale, firstAmount);
    REQUIRE(snappedScale == 1.0);

    const double nextAmount = motion.Add(direction);
    REQUIRE(std::abs(nextAmount) > std::abs(firstAmount));
    const double expectedScale = direction > 0.0 ? 1.096 : 1.0 / 1.096;
    REQUIRE(OIV::ViewActionController::RelativeZoom(snappedScale, nextAmount) == Catch::Approx(expectedScale));
}
