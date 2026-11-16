#pragma once
#include <LWS/Window.hpp>
#include <cstdint>
#include <span>

namespace OIV
{
    enum class WindowPlacementHint
    {
        NearOwner,
        CenterInOwner
    };

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
    // Supply the owner's monitor first; prefer it whenever the window fits on that screen.
    LWS::Point ResolveWindowPosition(std::span<const PlacementArea> workAreas, PlacementRect owner,
                                     LWS::Size windowSize, int32_t gap = 0,
                                     WindowPlacementHint hint = WindowPlacementHint::NearOwner);
    LWS::Point ResolveWindowPosition(PlacementRect workArea, PlacementRect owner, LWS::Size windowSize, int32_t gap = 0,
                                     WindowPlacementHint hint = WindowPlacementHint::NearOwner);
    // Limit growth to the remaining work area at the current position; retain explicit minimum sizes.
    LWS::LogicalSize FitWindowClientSize(const LWS::Window& window, LWS::LogicalSize desired);

    // CenterInOwner centers a fitting window without covering the entire owner; otherwise use nearby placement.
    // Places an existing top-level window near its owner when the platform supports absolute placement.
    LWS::Result PlaceWindowNear(LWS::Window& window, LWS::Window& owner,
                                WindowPlacementHint hint = WindowPlacementHint::NearOwner);
}  // namespace OIV
