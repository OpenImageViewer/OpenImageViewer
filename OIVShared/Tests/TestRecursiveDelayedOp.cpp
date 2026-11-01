#include <catch2/catch_test_macros.hpp>

#include <OIVShared/RecursiveDelayOp.h>

TEST_CASE("RecursiveDelayedOp delays callbacks until the outer operation ends", "[Shared]")
{
    int callbackCount = 0;
    OIV::RecursiveDelayedOp operation([&]() { ++callbackCount; });

    operation.Begin();
    operation.Begin();
    operation.End();
    REQUIRE(callbackCount == 0);

    operation.End();
    REQUIRE(callbackCount == 1);

    operation.Begin();
    operation.End(false);
    REQUIRE(callbackCount == 1);
}
