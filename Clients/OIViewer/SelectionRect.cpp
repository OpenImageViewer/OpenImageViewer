#include "SelectionRect.h"
#include <LLUtils/Exception.h>

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

    LLUtils::Corner SelectionRect::OppositeCorner(const LLUtils::Corner corner) const
    {
        using namespace LLUtils;
        switch (corner)
        {
        case Corner::TopRight:
            return BottomLeft;
        case Corner::BottomLeft:
            return TopRight;
        case Corner::TopLeft:
            return BottomRight;
        case Corner::BottomRight:
            return TopLeft;
        case Corner::None:
            LL_EXCEPTION(Exception::ErrorCode::LogicError, "corner must be set to get opposite corner");
        default:
            LL_EXCEPTION_UNEXPECTED_VALUE;
        }
    }

    LLUtils::Corner SelectionRect::GetClosestCorner(const LLUtils::PointI32& point) const
    {
        using namespace LLUtils;
        std::array< decltype(fSelectionRect)::Point_Type::point_type, 4> distances =
        {
              point.DistanceSquared(fSelectionRect.GetCorner(Corner::TopLeft))
            , point.DistanceSquared(fSelectionRect.GetCorner(Corner::BottomRight))
            , point.DistanceSquared(fSelectionRect.GetCorner(Corner::TopRight))
            , point.DistanceSquared(fSelectionRect.GetCorner(Corner::BottomLeft))
        };

        auto dist = std::distance(distances.begin(), std::min_element(distances.begin(), distances.end()));

        switch (dist)
        {
        case 0:
            return Corner::TopLeft;
        case 1:
            return Corner::BottomRight;
        case 2:
            return Corner::TopRight;
        case 3:
            return Corner::BottomLeft;
        }

        LL_EXCEPTION(Exception::ErrorCode::LogicError, "Unexpected value");
    }

    SelectionRect::SelectionRect(SelectionRectChangedCallback callback) : fCallback(callback)
    {

    }

    void SelectionRect::SetSelection(const Operation operation, const LLUtils::PointI32& posWindowSpace)
    {
        
        //Corner constants
        constexpr int CornerDragFactor = 6;
        constexpr double InflationFactor = 0.1;
        constexpr int MinInflation = 5;
        
        //Edge constants
        constexpr int EdgeDragFactor = 8;
        constexpr int minCaptureWidth = 8;

        switch (operation)
        {
        case Operation::NoOp:
            return; // return - Don't change state

        case Operation::BeginDrag:

            //Begin drawing the selection rect
            if (fOperation == Operation::NoOp)
            {
                fSelectStartPoint = posWindowSpace;
                fOperation = operation;
            }

            //Begin Dragging or resizing the selection rect
            if (fOperation == Operation::EndDrag)
            {
                decltype(fSelectionRect)::Point_Type inflationRate =
                {   std::max(MinInflation, static_cast<int>(fSelectionRect.GetWidth() * InflationFactor))
                  , std::max(MinInflation, static_cast<int>(fSelectionRect.GetHeight() * InflationFactor))
                };

                decltype(fSelectionRect) infaltedRect = fSelectionRect.Infalte(inflationRate.x, inflationRate.y);
                
                const bool isInside = infaltedRect.IsInside(posWindowSpace);

                if (isInside)
                {
                    LLUtils::Corner corner = GetClosestCorner(posWindowSpace);

                    decltype(fSelectionRect)::Point_Type distanceToCorner = (infaltedRect.GetCorner(corner) - posWindowSpace).Abs();
                    const bool shouldDragCorner = distanceToCorner.x < fSelectionRect.GetWidth() / CornerDragFactor && distanceToCorner.y < fSelectionRect.GetHeight() / CornerDragFactor;

                    if (shouldDragCorner == true)
                    {
                        //Selection rect already visible, re-drag
                        fOperation = Operation::BeginDrag;
                        fSelectStartPoint = fSelectionRect.GetCorner(OppositeCorner(corner));
                        fLockMode = LockMode::NoLock;
                    }
                    else
                    {
                        int captureWidth = std::max(minCaptureWidth, fSelectionRect.GetWidth() / EdgeDragFactor);
                        int captureHeight = std::max(minCaptureWidth, fSelectionRect.GetHeight() / EdgeDragFactor);
                        LLUtils::PointI32 minimum = fSelectionRect.GetCorner(LLUtils::Corner::TopLeft);
                        LLUtils::PointI32 maximum = fSelectionRect.GetCorner(LLUtils::Corner::BottomRight);
 
                        // resize from lower edge.
                        if (maximum.y - posWindowSpace.y < captureHeight)
                        {
                            fOperation = Operation::BeginDrag;
                            fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::Corner::TopLeft);
                            fLockMode = LockMode::LockWidth;
                        }
                        // resize from upper edge.
                        else if (posWindowSpace.y - minimum.y < captureHeight)
                        {
                            fOperation = Operation::BeginDrag;
                            fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::Corner::BottomRight);
                            fLockMode = LockMode::LockWidth;
                        }
                        // resize from right edge.
                        else if (maximum.x - posWindowSpace.x < captureWidth)
                        {
                            fOperation = Operation::BeginDrag;
                            fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::Corner::TopLeft);
                            fLockMode = LockMode::LockHeight;
                        }
                        // resize from left edge.
                        else if (posWindowSpace.x - minimum.x < captureWidth)
                        {
                            fOperation = Operation::BeginDrag;
                            fSelectStartPoint = fSelectionRect.GetCorner(LLUtils::Corner::TopRight);
                            fLockMode = LockMode::LockHeight;
                        }
                        else
                        {
                            //Move the rectangle
                            fSelectEndPoint = posWindowSpace;
                            fLockMode = LockMode::NoLock;
                        }
                    }
                }
                else
                {
                    //outside of the rectange - start a new selection rect.
                    fSelectStartPoint = posWindowSpace;
                    fOperation = Operation::BeginDrag;
                    fLockMode = LockMode::NoLock;
                }

            }
            break;


        case Operation::Drag:
            {
                //Changing the size of the selection rect
                if (fOperation == Operation::BeginDrag || fOperation == Operation::Drag)
                {
                    using namespace LLUtils;
                    decltype(fSelectionRect)::Point_Type topLeft = fSelectionRect.GetCorner(LLUtils::Corner::TopLeft);
                    decltype(fSelectionRect)::Point_Type bottomRight = fSelectionRect.GetCorner(LLUtils::Corner::BottomRight);

                    auto diff = (posWindowSpace - fSelectStartPoint).Abs();

                    if (diff.x != 0 && diff.y != 0)
                    {

                        PointI32 p0 = {
                           fLockMode == LockMode::LockWidth ? topLeft.x : std::min<PointI32::point_type>(posWindowSpace.x, fSelectStartPoint.x),
                           fLockMode == LockMode::LockHeight ? topLeft.y : std::min<PointI32::point_type>(posWindowSpace.y, fSelectStartPoint.y)
                        };
                        PointI32 p1 = {
                           fLockMode == LockMode::LockWidth ? bottomRight.x : std::max<PointI32::point_type>(posWindowSpace.x, fSelectStartPoint.x),
                           fLockMode == LockMode::LockHeight ? bottomRight.y : std::max<PointI32::point_type>(posWindowSpace.y, fSelectStartPoint.y)
                        };
                        fSelectionRect = { p0,p1 };
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

            //Update operation state only when finshing setting the selection rect.
            if (fOperation == Operation::Drag)
                fOperation = operation;


            break;
        case Operation::CancelSelection:
            fOperation = Operation::NoOp;
            fLockMode = LockMode::NoLock;
            NotifySelectionRectChanged(false);
            break;
        }
    }

    void SelectionRect::UpdateSelection(const LLUtils::RectI32 & selectionRect)
    {
        fSelectionRect = selectionRect;
        NotifySelectionRectChanged(true);
    }
}
