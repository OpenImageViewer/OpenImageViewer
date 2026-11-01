#include <System.h>
#include <SystemThreadPolicy.h>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Memory operation thread count supports common CPU topologies", "[system][threads]")
{
    REQUIRE(OIV::internal::CalculateIdealNumThreadsForMemoryOperations(8, 8) == 8);
    REQUIRE(OIV::internal::CalculateIdealNumThreadsForMemoryOperations(8, 16) == 12);
    REQUIRE(OIV::internal::CalculateIdealNumThreadsForMemoryOperations(24, 32) == 24);
}

TEST_CASE("Memory operation thread count has safe topology fallbacks", "[system][threads]")
{
    REQUIRE(OIV::internal::CalculateIdealNumThreadsForMemoryOperations(0, 0) == 1);
    REQUIRE(OIV::internal::CalculateIdealNumThreadsForMemoryOperations(0, 12) == 12);
    REQUIRE(OIV::internal::CalculateIdealNumThreadsForMemoryOperations(16, 0) == 16);
    REQUIRE(OIV::internal::CalculateIdealNumThreadsForMemoryOperations(16, 8) == 8);
}

TEST_CASE("Detected CPU topology always produces a usable thread count", "[system][threads]")
{
    REQUIRE(OIV::System::GetIdealNumThreadsForMemoryOperations() >= 1);
}
