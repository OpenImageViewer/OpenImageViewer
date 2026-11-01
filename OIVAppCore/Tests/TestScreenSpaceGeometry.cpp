#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include <OIVAppCore/ScreenSpaceGeometry.h>

namespace Geometry = OIV::ScreenSpaceGeometry;

TEST_CASE("Screen-space plans retain crop placement and sampling rules", "[AppCore][ScreenSpaceGeometry]")
{
    Geometry::View view{
        .imageSize    = {32768, 32},
        .fallbackSize = {16384, 16},
        .viewportSize = {25, 20},
        .scale        = {2.5, 2.5},
        .offset       = {-25.0, -5.0},
    };
    const auto native = Geometry::BuildPlan(view, 0.001, Geometry::DefaultPixelBufferTargetBytes,
                                            Geometry::DefaultTexelThresholdPixels);
    REQUIRE(native.has_value());
    CHECK(native->sourceRect.GetCorner(LLUtils::Corner::TopLeft) == LLUtils::PointI32{9, 1});
    CHECK(native->sourceRect.GetCorner(LLUtils::Corner::BottomRight) == LLUtils::PointI32{21, 11});
    CHECK(native->textureSize == LLUtils::PointI32{12, 10});
    CHECK(Geometry::CanReuse(view, *native, 0.0, Geometry::DefaultTexelThresholdPixels));

    view               = {.imageSize    = {163840, 20},
                          .fallbackSize = {16384, 2},
                          .viewportSize = {20, 4},
                          .scale        = {0.5, 0.5},
                          .offset       = {-10.0, -1.0}};
    const auto reduced = Geometry::BuildPlan(view, 0.001, Geometry::DefaultPixelBufferTargetBytes,
                                             Geometry::DefaultTexelThresholdPixels);
    REQUIRE(reduced.has_value());
    CHECK(reduced->sourceRect.GetCorner(LLUtils::Corner::TopLeft) == LLUtils::PointI32{19, 1});
    CHECK(reduced->sourceRect.GetCorner(LLUtils::Corner::BottomRight) == LLUtils::PointI32{61, 11});
    CHECK(reduced->textureSize == LLUtils::PointI32{21, 5});
}

TEST_CASE("Screen-space plans gate empty ordinary and offscreen views", "[AppCore][ScreenSpaceGeometry]")
{
    Geometry::View view{.imageSize    = {81920, 1},
                        .fallbackSize = {16384, 1},
                        .viewportSize = {20, 20},
                        .scale        = {0.999, 0.999}};
    CHECK_FALSE(Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes,
                                    Geometry::DefaultTexelThresholdPixels));
    view.scale      = {1.0, 1.0};
    const auto plan = Geometry::BuildPlan(view, 0.001, Geometry::DefaultPixelBufferTargetBytes,
                                          Geometry::DefaultTexelThresholdPixels);
    REQUIRE(plan);
    CHECK(plan->textureSize == LLUtils::PointI32{21, 1});
    for (const auto offset :
         {LLUtils::PointF64{21, 0}, LLUtils::PointF64{-81921, 0}, LLUtils::PointF64{0, 21}, LLUtils::PointF64{0, -2}})
    {
        view.offset = offset;
        CHECK_FALSE(Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes,
                                        Geometry::DefaultTexelThresholdPixels));
        CHECK_FALSE(Geometry::CanReuse(view, *plan, 0.25, Geometry::DefaultTexelThresholdPixels));
    }
    view.offset       = {};
    view.viewportSize = {};
    CHECK_FALSE(Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes,
                                    Geometry::DefaultTexelThresholdPixels));
    view.viewportSize = {20, 20};
    view.imageSize    = view.fallbackSize;
    CHECK_FALSE(Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes,
                                    Geometry::DefaultTexelThresholdPixels));
}

TEST_CASE("Screen-space plan margins depend on viewport work and respect resource limits",
          "[AppCore][ScreenSpaceGeometry]")
{
    struct Case
    {
        double zoom;
        double factor;
        LLUtils::PointI32 expected;
    };
    const auto test = GENERATE(Case{0.25, 16, {17, 9}}, Case{0.5, 4, {17, 9}}, Case{0.5, 9, {25, 13}},
                               Case{0.5, 16, {33, 17}}, Case{1, 16, {34, 18}});
    Geometry::View view{.imageSize    = {327680, 64},
                        .fallbackSize = {16384, 3},
                        .viewportSize = {16, 8},
                        .scale        = {test.zoom, test.zoom},
                        .offset       = {-1000, -6}};
    const auto plan = Geometry::BuildPlan(view, test.factor, Geometry::DefaultPixelBufferTargetBytes,
                                          Geometry::DefaultTexelThresholdPixels);
    REQUIRE(plan);
    CHECK(plan->textureSize == test.expected);
}

TEST_CASE("Screen-space plan memory limits trim margins without losing required pixels",
          "[AppCore][ScreenSpaceGeometry]")
{
    Geometry::View view{.imageSize    = {16385, 16385},
                        .fallbackSize = {16384, 16384},
                        .viewportSize = {30000, 10000},
                        .scale        = {5, 5}};
    auto plan = Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes,
                                    Geometry::DefaultTexelThresholdPixels);
    REQUIRE(plan);
    CHECK(static_cast<uint64_t>(plan->textureSize.x) * plan->textureSize.y * 4 <= 64 * 1024 * 1024);
    CHECK(plan->textureSize.x > 6001);
    CHECK(plan->textureSize.y > 2001);
    view.viewportSize = {45000, 15000};
    plan              = Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes,
                                            Geometry::DefaultTexelThresholdPixels);
    REQUIRE(plan);
    CHECK(plan->textureSize == LLUtils::PointI32{9001, 3001});

    view = {.imageSize = {81920, 20}, .fallbackSize = {16384, 4}, .viewportSize = {30000, 30}};
    plan = Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes,
                               Geometry::DefaultTexelThresholdPixels);
    REQUIRE(plan);
    CHECK(plan->textureSize == LLUtils::PointI32{16384, 20});
    CHECK(Geometry::CanReuse(view, *plan, 0.25, Geometry::DefaultTexelThresholdPixels));
}

TEST_CASE("Screen-space reuse separates coverage tolerance and native resolution", "[AppCore][ScreenSpaceGeometry]")
{
    Geometry::View view{.imageSize    = {327680, 128},
                        .fallbackSize = {16384, 6},
                        .viewportSize = {20, 10},
                        .scale        = {0.5, 0.5},
                        .offset       = {-50, -20}};
    const Geometry::Plan cached{.sourceRect = {{80, 20}, {180, 80}}, .textureSize = {50, 30}};
    CHECK(Geometry::CanReuse(view, cached, 0.0, Geometry::DefaultTexelThresholdPixels));
    view.scale  = {0.625, 0.625};
    view.offset = {-62.5, -25};
    CHECK_FALSE(Geometry::CanReuse(view, cached, 0.0, Geometry::DefaultTexelThresholdPixels));
    CHECK_FALSE(Geometry::CanReuse(
        view, cached, 0.25,
        Geometry::DefaultTexelThresholdPixels));  // Rounded width 63 exceeds the 62.5 upper sample allowance.
    view.scale  = {0.62, 0.62};
    view.offset = {-62, -24.8};
    CHECK(Geometry::CanReuse(view, cached, 0.25, Geometry::DefaultTexelThresholdPixels));
    view.scale  = {0.4, 0.4};
    view.offset = {-40, -16};
    CHECK(Geometry::CanReuse(view, cached, 0.25, Geometry::DefaultTexelThresholdPixels));
    view.scale  = {0.38, 0.38};
    view.offset = {-38, -15.2};
    CHECK_FALSE(Geometry::CanReuse(view, cached, 0.25, Geometry::DefaultTexelThresholdPixels));
    view.scale  = {1, 1};
    view.offset = {-100, -40};
    CHECK_FALSE(Geometry::CanReuse(view, cached, 10.0, Geometry::DefaultTexelThresholdPixels));
    const Geometry::Plan native{.sourceRect = cached.sourceRect, .textureSize = {100, 60}};
    CHECK(Geometry::CanReuse(view, native, 0.0, Geometry::DefaultTexelThresholdPixels));
    view.scale  = {2, 2};
    view.offset = {-200, -80};
    CHECK(Geometry::CanReuse(view, native, 0.0, Geometry::DefaultTexelThresholdPixels));
    view.offset = {-600, -80};
    CHECK_FALSE(Geometry::CanReuse(view, native, 0.25, Geometry::DefaultTexelThresholdPixels));
}

TEST_CASE("Screen-space activation uses the configured fractional threshold in either axis",
          "[AppCore][ScreenSpaceGeometry]")
{
    const auto scale = GENERATE(LLUtils::PointF64{1.25, 1.0}, LLUtils::PointF64{1.0, 1.25});
    const Geometry::View view{.imageSize    = {32768, 32768},
                              .fallbackSize = {16384, 16384},
                              .viewportSize = {20, 20},
                              .scale        = scale};
    const auto plan = Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes, 2.5);
    REQUIRE(plan);
    for (double threshold : {2.49, 2.5, 2.51})
    {
        CAPTURE(threshold);
        CHECK(Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes, threshold).has_value() ==
              (threshold <= 2.5));
        CHECK(Geometry::CanReuse(view, *plan, 0.25, threshold) == (threshold <= 2.5));
    }
}

TEST_CASE("Screen-space pixel-buffer targets trim optional margins only", "[AppCore][ScreenSpaceGeometry]")
{
    const Geometry::View view{.imageSize    = {163840, 128},
                              .fallbackSize = {16384, 16},
                              .viewportSize = {16, 8},
                              .scale        = {0.5, 0.5},
                              .offset       = {-1000, -16}};
    const auto full      = Geometry::BuildPlan(view, 16.0, Geometry::DefaultPixelBufferTargetBytes, 5.0);
    const auto trimmed   = Geometry::BuildPlan(view, 16.0, 1200, 5.0);
    const auto mandatory = Geometry::BuildPlan(view, 16.0, 1, 5.0);
    REQUIRE(full);
    REQUIRE(trimmed);
    REQUIRE(mandatory);
    CHECK(full->textureSize == LLUtils::PointI32{33, 17});
    CHECK(trimmed->textureSize.x * trimmed->textureSize.y * 4 <= 1200);
    CHECK(trimmed->textureSize.x > mandatory->textureSize.x);
    CHECK(trimmed->textureSize.x < full->textureSize.x);
    CHECK(mandatory->textureSize == LLUtils::PointI32{17, 9});
    CHECK(mandatory->sourceRect.IsInside(trimmed->sourceRect));
    CHECK(trimmed->sourceRect.IsInside(full->sourceRect));
}
