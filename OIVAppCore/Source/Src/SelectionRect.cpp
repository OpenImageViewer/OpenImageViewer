#include <OIVAppCore/SelectionRect.h>

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace OIV
{
    void SelectionRect::NotifySelectionRectChanged(bool isVisible) const
    {
        fCallback(fSelectionRect, isVisible);
    }

    const LLUtils::RectI32& SelectionRect::GetSelectionRect() const
    {
        return fSelectionRect;
    }

    void SelectionRect::SetContentScale(LLUtils::PointF64 scale)
    {
        fEdgeTolerance   = {std::max(1, static_cast<int>(std::lround(EdgeTolerance * scale.x))),
                            std::max(1, static_cast<int>(std::lround(EdgeTolerance * scale.y)))};
        fCornerTolerance = {std::max(1, static_cast<int>(std::lround(CornerTolerance * scale.x))),
                            std::max(1, static_cast<int>(std::lround(CornerTolerance * scale.y)))};
    }

    SelectionRect::Handle SelectionRect::HitTest(const LLUtils::PointI32& point) const
    {
        Handle handle = Handle::None;
        if (fOperation == Operation::EndDrag && !fSelectionRect.IsEmpty())
        {
            const auto minimum = fSelectionRect.GetCorner(LLUtils::TopLeft);
            const auto maximum = fSelectionRect.GetCorner(LLUtils::BottomRight);
            // Keep tolerances fixed in logical pixels, but reserve the middle of small selections for moving.
            const int horizontalLimit = (fSelectionRect.GetWidth() - 1) / 4;
            const int verticalLimit   = (fSelectionRect.GetHeight() - 1) / 4;
            const int cornerX         = std::min(fCornerTolerance.x, horizontalLimit);
            const int cornerY         = std::min(fCornerTolerance.y, verticalLimit);
            const int edgeX           = std::min(fEdgeTolerance.x, horizontalLimit);
            const int edgeY           = std::min(fEdgeTolerance.y, verticalLimit);
            const auto left           = std::abs(static_cast<int64_t>(point.x) - minimum.x);
            const auto right          = std::abs(static_cast<int64_t>(point.x) - maximum.x);
            const auto top            = std::abs(static_cast<int64_t>(point.y) - minimum.y);
            const auto bottom         = std::abs(static_cast<int64_t>(point.y) - maximum.y);
            const bool withinX        = point.x >= minimum.x && point.x <= maximum.x;
            const bool withinY        = point.y >= minimum.y && point.y <= maximum.y;

            // Corners take priority over edges, and only the remaining interior moves the rectangle.
            if (left <= cornerX && top <= cornerY)
                handle = Handle::TopLeft;
            else if (right <= cornerX && top <= cornerY)
                handle = Handle::TopRight;
            else if (left <= cornerX && bottom <= cornerY)
                handle = Handle::BottomLeft;
            else if (right <= cornerX && bottom <= cornerY)
                handle = Handle::BottomRight;
            else if (withinX && top <= edgeY)
                handle = Handle::Top;
            else if (withinX && bottom <= edgeY)
                handle = Handle::Bottom;
            else if (withinY && left <= edgeX)
                handle = Handle::Left;
            else if (withinY && right <= edgeX)
                handle = Handle::Right;
            else if (withinX && withinY)
                handle = Handle::Move;
        }
        return handle;
    }

    SelectionRect::SelectionRect(SelectionRectChangedCallback callback) : fCallback(callback) {}

    void SelectionRect::SetSelection(const Operation operation, const LLUtils::PointI32& posWindowSpace,
                                     std::optional<Handle> hitHandle)
    {
        switch (operation)
        {
            case Operation::NoOp:
                return;  // return - Don't change state

            case Operation::BeginDrag:

                // Begin drawing the selection rect
                if (fOperation == Operation::NoOp)
                {
                    fSelectStartPoint = posWindowSpace;
                    fOperation        = operation;
                }

                // Begin Dragging or resizing the selection rect
                if (fOperation == Operation::EndDrag)
                {
                    const auto handle = hitHandle ? *hitHandle : HitTest(posWindowSpace);
                    fLockMode         = LockMode::NoLock;
                    if (handle == Handle::Move)
                        fSelectEndPoint = posWindowSpace;
                    else
                    {
                        fOperation = Operation::BeginDrag;
                        switch (handle)
                        {
                            case Handle::TopLeft:
                                fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::BottomRight);
                                break;
                            case Handle::TopRight:
                                fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::BottomLeft);
                                break;
                            case Handle::BottomLeft:
                                fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::TopRight);
                                break;
                            case Handle::BottomRight:
                                fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::TopLeft);
                                break;
                            case Handle::Top:
                                fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::BottomRight);
                                fLockMode         = LockMode::LockWidth;
                                break;
                            case Handle::Bottom:
                                fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::TopLeft);
                                fLockMode         = LockMode::LockWidth;
                                break;
                            case Handle::Left:
                                fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::TopRight);
                                fLockMode         = LockMode::LockHeight;
                                break;
                            case Handle::Right:
                                fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::TopLeft);
                                fLockMode         = LockMode::LockHeight;
                                break;
                            default:
                                fSelectStartPoint = posWindowSpace;
                                break;
                        }
                    }
                }
                break;

            case Operation::Drag:
            {
                // Changing the size of the selection rect
                if (fOperation == Operation::BeginDrag || fOperation == Operation::Drag)
                {
                    using namespace LLUtils;
                    decltype(fSelectionRect)::Point_Type topLeft = fSelectionRect.GetCorner(LLUtils::Corner::TopLeft);
                    decltype(fSelectionRect)::Point_Type bottomRight = fSelectionRect.GetCorner(
                        LLUtils::Corner::BottomRight);

                    auto diff = (posWindowSpace - fSelectStartPoint).Abs();

                    if (diff.x != 0 && diff.y != 0)
                    {
                        PointI32 p0    = {fLockMode == LockMode::LockWidth
                                              ? topLeft.x
                                              : std::min<PointI32::point_type>(posWindowSpace.x, fSelectStartPoint.x),
                                          fLockMode == LockMode::LockHeight
                                              ? topLeft.y
                                              : std::min<PointI32::point_type>(posWindowSpace.y, fSelectStartPoint.y)};
                        PointI32 p1    = {fLockMode == LockMode::LockWidth
                                              ? bottomRight.x
                                              : std::max<PointI32::point_type>(posWindowSpace.x, fSelectStartPoint.x),
                                          fLockMode == LockMode::LockHeight
                                              ? bottomRight.y
                                              : std::max<PointI32::point_type>(posWindowSpace.y, fSelectStartPoint.y)};
                        fSelectionRect = {p0, p1};
                        NotifySelectionRectChanged(true);
                    }
                    fOperation = operation;
                }

                if (fOperation == Operation::EndDrag)
                {
                    using namespace LLUtils;
                    // Dragging the selection rect in the client window
                    fSelectionRect += posWindowSpace - fSelectEndPoint;
                    fSelectEndPoint = posWindowSpace;
                    NotifySelectionRectChanged(true);
                }
            }
            break;
            case Operation::EndDrag:

                // Update operation state only when finshing setting the selection rect.
                if (fOperation == Operation::Drag)
                    fOperation = operation;

                break;
            case Operation::CancelSelection:
                fSelectionRect    = {};
                fSelectStartPoint = {};
                fSelectEndPoint   = {};
                fOperation        = Operation::NoOp;
                fLockMode         = LockMode::NoLock;
                NotifySelectionRectChanged(false);
                break;
        }
    }

    void SelectionRect::UpdateSelection(const LLUtils::RectI32& selectionRect)
    {
        fSelectionRect = selectionRect;
        NotifySelectionRectChanged(true);
    }
}  // namespace OIV
