#include "MouseCaptureState.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Mouse capture starts inside and ends on release", "[input]")
{
    OIV::MouseCaptureState capture;

    capture.Update(LWS::MouseButton::Right, true, false);
    REQUIRE_FALSE(capture.IsCaptured(LWS::MouseButton::Right));

    capture.Update(LWS::MouseButton::Right, false, false);
    capture.Update(LWS::MouseButton::Right, true, true);
    REQUIRE(capture.IsCaptured(LWS::MouseButton::Right));

    capture.Update(LWS::MouseButton::Right, false, false);
    REQUIRE_FALSE(capture.IsCaptured(LWS::MouseButton::Right));
}

TEST_CASE("Mouse capture reset clears every captured button", "[input]")
{
    OIV::MouseCaptureState capture;
    capture.Update(LWS::MouseButton::Left, true, true);
    capture.Update(LWS::MouseButton::X1, true, true);

    capture.Reset();

    REQUIRE_FALSE(capture.IsCaptured(LWS::MouseButton::Left));
    REQUIRE_FALSE(capture.IsCaptured(LWS::MouseButton::X1));
}
