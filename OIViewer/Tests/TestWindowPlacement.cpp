#ifdef OIV_TEST_HAS_SETTINGS_EDITOR
    #include <catch2/catch_test_macros.hpp>
    #include "WindowPlacement.h"
    #include <array>

TEST_CASE("Auxiliary windows use available work area near the owner", "[oiviewer][window-placement]")
{
    using namespace OIV;
    SECTION("Enough room on the right")
    {
        const auto position = ResolveWindowPosition({0, 0, 1920, 1080}, {200, 100, 900, 850}, {640, 520}, 12);
        CHECK(position.x == 912);
        CHECK(position.y == 215);
    }
    SECTION("Use the left when the right edge is occupied")
    {
        const auto position = ResolveWindowPosition({0, 0, 1920, 1080}, {1200, 100, 1850, 800}, {640, 520}, 12);
        CHECK(position.x == 548);
        CHECK(position.y == 190);
    }
    SECTION("Keep a small owner visible")
    {
        const auto position = ResolveWindowPosition({0, 0, 1200, 900}, {900, 650, 1000, 750}, {640, 520}, 12);
        CHECK(position.x + 640 <= 900);
        CHECK(position.y >= 0);
        CHECK(position.y + 520 <= 900);
    }
    SECTION("Use monitor coordinates on the left of the primary display")
    {
        const auto position = ResolveWindowPosition({-1920, 0, 0, 1080}, {-1800, 100, -1000, 800}, {640, 520}, 12);
        CHECK(position.x == -988);
        CHECK(position.y == 190);
    }
    SECTION("Keep a window within a full-screen owner's work area")
    {
        const auto position = ResolveWindowPosition({0, 0, 1280, 720}, {0, 0, 1280, 720}, {640, 520}, 12);
        CHECK(position.x >= 0);
        CHECK(position.y >= 0);
        CHECK(position.x + 640 <= 1280);
        CHECK(position.y + 520 <= 720);
    }
    SECTION("Keep dialogs on the same monitor as a maximized viewer")
    {
        const std::array areas{PlacementArea{{0, 0, 1920, 1080}}, PlacementArea{{1920, 0, 3840, 1080}}};
        const auto position = ResolveWindowPosition(areas, {0, 0, 1920, 1080}, {640, 520}, 12);
        CHECK(position.x == 1280);
        CHECK(position.y == 280);
    }
    SECTION("Prefer a complete window over a smaller adjacent monitor")
    {
        const std::array areas{PlacementArea{{0, 0, 1920, 1080}}, PlacementArea{{1920, 0, 2420, 400}}};
        const auto position = ResolveWindowPosition(areas, {0, 0, 1920, 1080}, {640, 520}, 12);
        CHECK(position.x >= 0);
        CHECK(position.x + 640 <= 1920);
        CHECK(position.y >= 0);
        CHECK(position.y + 520 <= 1080);
    }
    SECTION("A scaled window must fit the adjacent monitor")
    {
        const std::array areas{PlacementArea{{0, 0, 1920, 1040}}, PlacementArea{{1920, 0, 3840, 1040}, 2.0}};
        const auto position = ResolveWindowPosition(areas, {0, 0, 1920, 1040}, {656, 559}, 12);
        CHECK(position.x >= 0);
        CHECK(position.x + 656 <= 1920);
        CHECK(position.y >= 0);
        CHECK(position.y + 559 <= 1040);
    }
    SECTION("Prefer the owner monitor even when another scaled monitor has room")
    {
        const std::array areas{PlacementArea{{0, 0, 1920, 1040}}, PlacementArea{{1920, 0, 4480, 1400}, 2.0}};
        const auto position = ResolveWindowPosition(areas, {0, 0, 1920, 1040}, {656, 559}, 12);
        CHECK(position.x >= 0);
        CHECK(position.y >= 0);
        CHECK(position.x + 656 <= 1920);
        CHECK(position.y + 559 <= 1040);
    }
    SECTION("Use another monitor when the owner monitor cannot fit the window")
    {
        const std::array areas{PlacementArea{{0, 0, 500, 400}}, PlacementArea{{500, 0, 2420, 1080}}};
        const auto position = ResolveWindowPosition(areas, {0, 0, 500, 400}, {640, 520}, 12);
        CHECK(position.x == 512);
        CHECK(position.y == 0);
    }
    SECTION("Prefer the owner's non-primary monitor at negative screen coordinates")
    {
        const std::array areas{PlacementArea{{-1920, 0, 0, 1080}}, PlacementArea{{0, 0, 1920, 1080}}};
        const auto position = ResolveWindowPosition(areas, {-1920, 0, 0, 1080}, {640, 520}, 12);
        CHECK(position.x >= -1920);
        CHECK(position.x + 640 <= 0);
        CHECK(position.y >= 0);
        CHECK(position.y + 520 <= 1080);
    }
    SECTION("An oversized window starts at the work area origin")
    {
        const auto position = ResolveWindowPosition({100, 50, 600, 450}, {250, 150, 350, 250}, {640, 520}, 12);
        CHECK(position.x == 100);
        CHECK(position.y == 50);
    }
}
TEST_CASE("Centered placement hints preserve the owner and monitor", "[oiviewer][window-placement]")
{
    using namespace OIV;
    constexpr auto hint = WindowPlacementHint::CenterInOwner;
    SECTION("Center a fitting dialog")
    {
        const auto position = ResolveWindowPosition({0, 0, 1920, 1080}, {200, 100, 1200, 900}, {400, 300}, 12, hint);
        CHECK(position.x == 500);
        CHECK(position.y == 350);
    }
    SECTION("Do not cover the whole owner")
    {
        const auto position = ResolveWindowPosition({0, 0, 1920, 1080}, {200, 100, 600, 400}, {400, 300}, 12, hint);
        CHECK(position.x == 612);
        CHECK(position.y == 100);
    }
    SECTION("A larger dialog falls back to nearby placement")
    {
        const auto position = ResolveWindowPosition({0, 0, 1920, 1080}, {200, 100, 600, 400}, {640, 520}, 12, hint);
        CHECK(position.x == 612);
        CHECK(position.y == 0);
    }
    SECTION("Center using the owner monitor's scale and negative coordinates")
    {
        const std::array areas{PlacementArea{{-1920, 0, 0, 1080}, 1.5}, PlacementArea{{0, 0, 1920, 1080}}};
        const auto position = ResolveWindowPosition(areas, {-1800, 100, -600, 1000}, {400, 300}, 12, hint);
        CHECK(position.x == -1500);
        CHECK(position.y == 325);
    }
    SECTION("Off-screen centering falls back to visible placement")
    {
        const PlacementRect work{0, 0, 1920, 1080}, owner{-800, 100, 200, 900};
        const auto position = ResolveWindowPosition(work, owner, {400, 300}, 12, hint);
        CHECK(position.x == 212);
        CHECK(position.y == 350);
    }
}

#endif
