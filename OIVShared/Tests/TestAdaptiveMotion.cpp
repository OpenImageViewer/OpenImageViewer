#include <catch2/catch_all.hpp>

#include <OIVShared/AdaptiveMotion.h>

TEST_CASE("AdaptiveMotion can use deterministic elapsed time", "[Shared]")
{
    double elapsedSeconds = 0.0;
    OIV::AdaptiveMotion motion(2.0, 1.0, 1.0, [&]() { return elapsedSeconds; });

    REQUIRE(motion.Add(1.0) == Catch::Approx(4.0));

    elapsedSeconds = 1.0;
    REQUIRE(motion.Add(1.0) == Catch::Approx(9.0));
}

TEST_CASE("AdaptiveMotion reset begins a new fine interaction", "[Shared]")
{
    OIV::AdaptiveMotion motion(1.0, 1.0, 5.0, [] { return 0.0; });
    REQUIRE(motion.Add(1) == 1.0);
    REQUIRE(motion.Add(1) == 4.0);
    motion.Reset();
    REQUIRE(motion.Add(1) == 1.0);
}
