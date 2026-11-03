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
    SECTION("Use an adjacent monitor instead of covering a maximized viewer")
    {
        const std::array areas{PlacementArea{{0, 0, 1920, 1080}}, PlacementArea{{1920, 0, 3840, 1080}}};
        const auto position = ResolveWindowPosition(areas, {0, 0, 1920, 1080}, {640, 520}, 12);
        CHECK(position.x == 1932);
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
        const std::array areas{PlacementArea{{0, 0, 1920, 1040}},
                               PlacementArea{{1920, 0, 3840, 1040}, 2.0}};
        const auto position = ResolveWindowPosition(areas, {0, 0, 1920, 1040}, {656, 559}, 12);
        CHECK(position.x >= 0);
        CHECK(position.x + 656 <= 1920);
        CHECK(position.y >= 0);
        CHECK(position.y + 559 <= 1040);
    }
    SECTION("Use the adjacent monitor when the scaled window fits")
    {
        const std::array areas{PlacementArea{{0, 0, 1920, 1040}},
                               PlacementArea{{1920, 0, 4480, 1400}, 2.0}};
        const auto position = ResolveWindowPosition(areas, {0, 0, 1920, 1040}, {656, 559}, 12);
        CHECK(position.x == 1944);
        CHECK(position.y >= 0);
        CHECK(position.x + 1312 <= 4480);
        CHECK(position.y + 1118 <= 1400);
    }
    SECTION("An oversized window starts at the work area origin")
    {
        const auto position = ResolveWindowPosition({100, 50, 600, 450}, {250, 150, 350, 250}, {640, 520}, 12);
        CHECK(position.x == 100);
        CHECK(position.y == 50);
    }
}
#endif
