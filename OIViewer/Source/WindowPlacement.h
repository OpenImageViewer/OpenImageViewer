#pragma once
#include <LWS/Window.hpp>
#include <cstdint>
#include <span>

namespace OIV
{
    struct PlacementRect
    {
        int32_t left, top, right, bottom;
    };

    struct PlacementArea
    {
        PlacementRect bounds;
        double scale = 1.0;
    };

    // Bounds use screen coordinates. Window size and gap are logical units scaled per work area.
    LWS::Point ResolveWindowPosition(std::span<const PlacementArea> workAreas, PlacementRect owner,
                                     LWS::Size windowSize, int32_t gap = 0);
    LWS::Point ResolveWindowPosition(PlacementRect workArea, PlacementRect owner, LWS::Size windowSize,
                                     int32_t gap = 0);
    // Places an existing top-level window near its owner when the platform supports absolute placement.
    LWS::Result PlaceWindowNear(LWS::Window& window, LWS::Window& owner);
}
