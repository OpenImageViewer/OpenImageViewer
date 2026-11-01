#include <catch2/catch_all.hpp>

#include <OIVShared/ViewTransformController.h>

TEST_CASE("ViewTransformController calculates fit, center, and coordinate transforms", "[Shared]")
{
    REQUIRE(OIV::ViewTransformController::FitScale({800.0, 600.0}, {400.0, 400.0}) == Catch::Approx(1.5));

    const LLUtils::PointF64 offset = OIV::ViewTransformController::CenterOffset({400.0, 300.0}, {200.0, 100.0});
    REQUIRE(offset == LLUtils::PointF64{300.0, 250.0});

    const LLUtils::PointF64 clientPoint = OIV::ViewTransformController::ImageToClient(LLUtils::PointF64{10.0, 20.0},
                                                                                      2.0, {5.0, 7.0});
    REQUIRE(clientPoint == LLUtils::PointF64{25.0, 47.0});
    REQUIRE(OIV::ViewTransformController::ClientToImage(clientPoint, 2.0, {5.0, 7.0}) == LLUtils::PointF64{10.0, 20.0});
}

TEST_CASE("ViewTransformController bounds offsets using configured margins", "[Shared]")
{
    REQUIRE(OIV::ViewTransformController::ResolveOffset({500.0, -500.0}, {100.0, 100.0}, {300.0, 300.0},
                                                        {0.75, 0.75}) == LLUtils::PointF64{75.0, -275.0});

    REQUIRE(OIV::ViewTransformController::ResolveOffset({-100.0, 100.0}, {300.0, 300.0}, {100.0, 100.0},
                                                        {0.75, 0.75}) == LLUtils::PointF64{-75.0, 100.0});
}
