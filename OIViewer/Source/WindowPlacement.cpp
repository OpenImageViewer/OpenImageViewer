#include "WindowPlacement.h"
#include <algorithm>
#include <array>
#include <limits>
#include <tuple>
#include <vector>
#include <cmath>
#ifdef LWS_HAS_WIN32_BACKEND
    #include <LWS/Win32/WindowExtensions.hpp>
#endif

namespace OIV
{
    namespace
    {
        LWS::Size ScaleSize(LWS::Size size, double scale)
        {
            return {static_cast<int32_t>(std::lround(size.x * scale)),
                    static_cast<int32_t>(std::lround(size.y * scale))};
        }
    }  // namespace

    LWS::Point ResolveWindowPosition(std::span<const PlacementArea> workAreas, PlacementRect owner,
                                     LWS::Size windowSize, int32_t gap, WindowPlacementHint hint)
    {
        using Coordinate           = int64_t;
        const Coordinate ownerLeft = owner.left, ownerTop = owner.top, ownerRight = owner.right,
                         ownerBottom = owner.bottom;
        // Keep the window on screen, then prefer the owner's physical monitor before
        // minimizing overlap and preserving the requested gap. The owner's work area is first.
        using Score = std::tuple<Coordinate, bool, Coordinate, Coordinate, Coordinate>;
        std::optional<Score> bestScore;
        LWS::Point best{owner.left, owner.top};
        for (const auto& area : workAreas)
        {
            const auto& bounds    = area.bounds;
            const Coordinate left = bounds.left, top = bounds.top, right = bounds.right, bottom = bounds.bottom;
            if (right <= left || bottom <= top)
                continue;
            const auto pixels         = ScaleSize(windowSize, area.scale);
            const Coordinate width    = std::max<int32_t>(pixels.x, 1);
            const Coordinate height   = std::max<int32_t>(pixels.y, 1);
            const Coordinate distance = std::max<int32_t>(static_cast<int32_t>(std::lround(gap * area.scale)), 0);
            const Coordinate centerX  = ownerLeft + (ownerRight - ownerLeft - width) / 2;
            const Coordinate centerY  = ownerTop + (ownerBottom - ownerTop - height) / 2;
            // Center only on the owner's monitor when the dialog fits inside both the owner and work area.
            // Larger dialogs retain the normal placement policy so the owner remains accessible.
            if (hint == WindowPlacementHint::CenterInOwner && &area == workAreas.data() &&
                width <= ownerRight - ownerLeft && height <= ownerBottom - ownerTop &&
                (width < ownerRight - ownerLeft || height < ownerBottom - ownerTop) && centerX >= left &&
                centerY >= top && centerX + width <= right && centerY + height <= bottom)
                return {static_cast<int32_t>(centerX), static_cast<int32_t>(centerY)};
            const std::array<std::pair<Coordinate, Coordinate>, 8> candidates{{{ownerRight + distance, centerY},
                                                                               {ownerLeft - distance - width, centerY},
                                                                               {centerX, ownerBottom + distance},
                                                                               {centerX, ownerTop - distance - height},
                                                                               {left, top},
                                                                               {right - width, top},
                                                                               {left, bottom - height},
                                                                               {right - width, bottom - height}}};
            for (const auto& [candidateX, candidateY] : candidates)
            {
                const Coordinate x            = std::clamp(candidateX, left, std::max(left, right - width));
                const Coordinate y            = std::clamp(candidateY, top, std::max(top, bottom - height));
                const Coordinate visibleWidth = std::max<Coordinate>(0, std::min(x + width, right) - std::max(x, left));
                const Coordinate visibleHeight = std::max<Coordinate>(0,
                                                                      std::min(y + height, bottom) - std::max(y, top));
                const Coordinate outside       = width * height - visibleWidth * visibleHeight;
                const Coordinate overlapWidth  = std::max<Coordinate>(0, std::min(x + width, ownerRight) -
                                                                             std::max(x, ownerLeft));
                const Coordinate overlapHeight = std::max<Coordinate>(0, std::min(y + height, ownerBottom) -
                                                                             std::max(y, ownerTop));
                const Coordinate overlap       = overlapWidth * overlapHeight;
                const Coordinate dx            = std::max<Coordinate>({0, ownerLeft - (x + width), x - ownerRight});
                const Coordinate dy            = std::max<Coordinate>({0, ownerTop - (y + height), y - ownerBottom});
                const Coordinate separation    = dx + dy;
                const Coordinate gapError      = separation >= distance ? separation - distance : distance - separation;
                const Score score{outside, &area != workAreas.data(), overlap, gapError, separation};
                if (!bestScore || score < *bestScore)
                {
                    bestScore = score;
                    best      = {static_cast<int32_t>(x), static_cast<int32_t>(y)};
                }
            }
        }
        return best;
    }

    LWS::Point ResolveWindowPosition(PlacementRect workArea, PlacementRect owner, LWS::Size windowSize, int32_t gap,
                                     WindowPlacementHint hint)
    {
        const PlacementArea area{workArea};
        return ResolveWindowPosition(std::span<const PlacementArea>(&area, 1), owner, windowSize, gap, hint);
    }

    LWS::LogicalSize FitWindowClientSize([[maybe_unused]] const LWS::Window& window, LWS::LogicalSize desired)
    {
#ifdef LWS_HAS_WIN32_BACKEND
        const auto hwnd = LWS::Win32::GetHwnd(window);
        const auto dpi  = LWS::Win32::GetDpi(window);
        MONITORINFO monitor{sizeof(monitor)};
        RECT bounds{}, client{};
        if (hwnd && dpi && GetMonitorInfoW(MonitorFromWindow(*hwnd, MONITOR_DEFAULTTONEAREST), &monitor) &&
            GetWindowRect(*hwnd, &bounds) && GetClientRect(*hwnd, &client))
        {
            const double scale    = *dpi / 96.0;
            const auto minimum    = window.GetClientSizeLimits().minimum;
            const int frameWidth  = bounds.right - bounds.left - client.right;
            const int frameHeight = bounds.bottom - bounds.top - client.bottom;
            const auto width      = static_cast<int32_t>(
                std::floor((monitor.rcWork.right - std::max(monitor.rcWork.left, bounds.left) - frameWidth) / scale));
            const auto height = static_cast<int32_t>(
                std::floor((monitor.rcWork.bottom - std::max(monitor.rcWork.top, bounds.top) - frameHeight) / scale));
            desired.x = std::max(minimum.x, std::min(desired.x, std::max(1, width)));
            desired.y = std::max(minimum.y, std::min(desired.y, std::max(1, height)));
        }
#endif
        return desired;
    }

    LWS::Result PlaceWindowNear(LWS::Window& window, LWS::Window& owner, WindowPlacementHint hint)
    {
        if (!window.IsCreated() || !owner.IsCreated())
            return LWS::Result::InvalidState;
        if (&window == &owner || &window.GetPlatformContext() != &owner.GetPlatformContext())
            return LWS::Result::InvalidArgument;
        if (window.IsPopup() || window.IsChild() || owner.GetShowState() == LWS::WindowShowState::Minimized)
            return LWS::Result::NotSupported;
        auto& platform       = owner.GetPlatformContext();
        const auto supported = platform.Supports(LWS::PlatformFeature::AbsoluteWindowPosition);
        if (!supported)
            return supported.error();
        if (!*supported)
            return LWS::Result::NotSupported;
        const auto ownerBounds = owner.GetScreenBounds();
        const auto childBounds = window.GetScreenBounds();
        const auto monitors    = platform.GetMonitors(true);
        if (!ownerBounds)
            return ownerBounds.error();
        if (!childBounds)
            return childBounds.error();
        if (!monitors)
            return monitors.error();
        if (monitors->empty())
            return LWS::Result::NotSupported;
#ifdef LWS_HAS_WIN32_BACKEND
        const auto childDpi = LWS::Win32::GetDpi(window);
        if (!childDpi)
            return childDpi.error();
        const double childScale = *childDpi / 96.0;
        const auto toRect       = [](const LWS::Rect& rect)
        {
            const auto first = rect.GetCorner(LLUtils::TopLeft);
            const auto last  = rect.GetCorner(LLUtils::BottomRight);
            return PlacementRect{first.x, first.y, last.x, last.y};
        };
        const auto parent         = toRect(*ownerBounds);
        const auto child          = toRect(*childBounds);
        const auto nearestMonitor = [&](PlacementRect bounds) -> const LWS::MonitorDesc&
        {
            const LWS::MonitorDesc* best = &monitors->front();
            int64_t bestOverlap          = -1;
            int64_t bestDistance         = std::numeric_limits<int64_t>::max();
            for (const auto& monitor : *monitors)
            {
                const auto area       = toRect(monitor.monitorRect);
                const int64_t width   = std::max<int64_t>(0, std::min<int64_t>(bounds.right, area.right) -
                                                                 std::max<int64_t>(bounds.left, area.left));
                const int64_t height  = std::max<int64_t>(0, std::min<int64_t>(bounds.bottom, area.bottom) -
                                                                 std::max<int64_t>(bounds.top, area.top));
                const int64_t overlap = width * height;
                const int64_t dx      = std::max<int64_t>(
                    {0, int64_t(area.left) - bounds.right, int64_t(bounds.left) - area.right});
                const int64_t dy = std::max<int64_t>(
                    {0, int64_t(area.top) - bounds.bottom, int64_t(bounds.top) - area.bottom});
                if (overlap > bestOverlap || (overlap == bestOverlap && dx + dy < bestDistance))
                {
                    best         = &monitor;
                    bestOverlap  = overlap;
                    bestDistance = dx + dy;
                }
            }
            return *best;
        };
        const auto& ownerMonitor = nearestMonitor(parent);
        const auto ownerArea     = toRect(ownerMonitor.workRect);
        if (parent.right <= ownerArea.left || parent.left >= ownerArea.right || parent.bottom <= ownerArea.top ||
            parent.top >= ownerArea.bottom)
            return LWS::Result::NotSupported;
        std::vector<PlacementArea> areas{{ownerArea, ownerMonitor.contentScale.x}};
        for (const auto& monitor : *monitors)
            if (monitor.handle != ownerMonitor.handle)
                areas.push_back({toRect(monitor.workRect), monitor.contentScale.x});
        constexpr int32_t gap  = 12;
        const auto targetSize  = ScaleSize({child.right - child.left, child.bottom - child.top}, 1.0 / childScale);
        auto position          = ResolveWindowPosition(areas, parent, targetSize, gap, hint);
        const auto coversOwner = [&](LWS::Point point, LWS::Size logicalSize)
        {
            const auto& monitor = nearestMonitor({point.x, point.y, point.x + 1, point.y + 1});
            const auto size     = ScaleSize(logicalSize, monitor.contentScale.x);
            return point.x <= parent.left && point.y <= parent.top && int64_t(point.x) + size.x >= parent.right &&
                   int64_t(point.y) + size.y >= parent.bottom;
        };
        std::optional<LWS::LogicalSize> resized;
        if (coversOwner(position, targetSize))
        {
            const auto metrics = window.GetClientAreaMetrics();
            const auto limits  = window.GetClientSizeLimits().minimum;
            if (metrics.pixels && metrics.logical.x > 0 && metrics.logical.y > 0)
            {
                const int32_t minWidth = std::min(
                    metrics.logical.x, std::max(limits.x, static_cast<int32_t>(std::ceil(metrics.logical.x * 0.75))));
                const int32_t minHeight = std::min(
                    metrics.logical.y, std::max(limits.y, static_cast<int32_t>(std::ceil(metrics.logical.y * 0.75))));
                const double borderWidth  = (child.right - child.left - metrics.pixels->x) / childScale;
                const double borderHeight = (child.bottom - child.top - metrics.pixels->y) / childScale;
                const std::array<LWS::LogicalSize, 3> sizes{
                    {{minWidth, metrics.logical.y}, {metrics.logical.x, minHeight}, {minWidth, minHeight}}};
                int64_t bestArea = 0;
                for (const auto size : sizes)
                {
                    const LWS::Size outer{static_cast<int32_t>(std::lround(size.x + borderWidth)),
                                          static_cast<int32_t>(std::lround(size.y + borderHeight))};
                    const auto candidate = ResolveWindowPosition(areas, parent, outer, gap, hint);
                    const int64_t area   = int64_t(size.x) * size.y;
                    if (!coversOwner(candidate, outer) && area > bestArea)
                    {
                        bestArea = area;
                        resized  = size;
                        position = candidate;
                    }
                }
            }
        }
        const auto moveTo = [&](LWS::Point pixels, std::optional<LWS::LogicalSize> size = std::nullopt) -> LWS::Result
        {
            const auto dpi = LWS::Win32::GetDpi(window);
            if (!dpi)
                return dpi.error();
            const double scale = *dpi / 96.0;
            return window.RequestPlacement({.position = LWS::Point{static_cast<int32_t>(std::lround(pixels.x / scale)),
                                                                   static_cast<int32_t>(std::lround(pixels.y / scale))},
                                            .clientSize = size});
        };
        if (const auto moved = moveTo(position, resized); moved != LWS::Result::Success)
            return moved;
        // Fit after the move so monitor DPI and native frame metrics are authoritative.
        const auto currentSize = window.GetClientAreaMetrics().logical;
        const auto fitted      = FitWindowClientSize(window, currentSize);
        if (fitted != currentSize)
            if (const auto result = window.RequestPlacement({.clientSize = fitted}); result != LWS::Result::Success)
                return result;
        // Crossing a DPI boundary can resize the window. Correct using its actual bounds and monitor.
        const auto actual = window.GetScreenBounds();
        if (!actual)
            return actual.error();
        const auto actualRect     = toRect(*actual);
        const auto& targetMonitor = nearestMonitor(actualRect);
        const auto targetArea     = toRect(targetMonitor.workRect);
        auto corrected            = ResolveWindowPosition(targetArea, parent,
                                                          {actualRect.right - actualRect.left, actualRect.bottom - actualRect.top},
                                                          static_cast<int32_t>(std::lround(gap * targetMonitor.contentScale.x)),
                                                          hint);
        // Absolute placement uses whole logical coordinates. Round inward at work-area
        // edges so conversion back to physical pixels cannot leave a one-pixel overflow.
        const double targetScale = LWS::Win32::GetDpi(window).value_or(96) / 96.0;
        const auto inward        = [targetScale](int32_t value, int32_t low, int32_t high)
        {
            const double first   = std::ceil(low / targetScale);
            const double last    = std::floor(high / targetScale);
            const double logical = first <= last ? std::clamp(std::round(value / targetScale), first, last) : first;
            return static_cast<int32_t>(std::lround(logical * targetScale));
        };
        corrected.x = inward(corrected.x, targetArea.left, targetArea.right - (actualRect.right - actualRect.left));
        corrected.y = inward(corrected.y, targetArea.top, targetArea.bottom - (actualRect.bottom - actualRect.top));
        if (corrected.x != actualRect.left || corrected.y != actualRect.top)
        {
            if (const auto moved = moveTo(corrected); moved != LWS::Result::Success)
                return moved;
            const auto placed = window.GetScreenBounds();
            if (!placed)
                return placed.error();
            const auto finalBounds = toRect(*placed);
            if (std::abs(int64_t(finalBounds.left) - corrected.x) > 2 ||
                std::abs(int64_t(finalBounds.top) - corrected.y) > 2)
                return LWS::Result::Failure;
        }
        return LWS::Result::Success;
#else
        return LWS::Result::NotSupported;
#endif
    }
}  // namespace OIV
