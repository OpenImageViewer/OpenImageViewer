#include "SelectionRect.h"
namespace OIV
{
    void SelectionRect::UpdateVisualSelectionRect() const
    {
        OIVCommands::SetSelectionRect(fSelectionRect);
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
            throw std::logic_error("Error, Corner must be set to get opposite corner");
        default:
            throw std::logic_error("Error, Unexpected or coruppted value");
        }
    }

    LLUtils::Corner SelectionRect::GetCorner(const LLUtils::PointI32& point) const
    {
        using namespace LLUtils;
        PointI32 dist = (point - fSelectionRect.GetCorner(Corner::TopLeft)).Abs();
        int width = fSelectionRect.GetWidth();
        int height = fSelectionRect.GetHeight();

        Corner corner = Corner::None;

        if (dist.x < width / 6 && dist.y < height / 6)
            corner = TopLeft;

        if (corner == Corner::None)
        {
            dist = (point - fSelectionRect.GetCorner(Corner::BottomRight)).Abs();
            if (dist.x < width / 6 && dist.y < height / 6)
            {
                corner = BottomRight;
            }
        }
        if (corner == Corner::None)
        {
            dist = (point - PointI32(fSelectionRect.GetCorner(Corner::BottomLeft))).Abs();
            if (dist.x < width / 6 && dist.y < height / 6)
            {
                corner = BottomLeft;
            }
        }
        if (corner == Corner::None)
        {
            dist = (point - fSelectionRect.GetCorner(Corner::TopRight)).Abs();
            if (dist.x < width / 6 && dist.y < height / 6)
            {
                corner = TopRight;
            }
        }

        return corner;
    }

    void SelectionRect::SetSelection(const Operation operation, const LLUtils::PointI32& posWindowSpace)
    {
        switch (operation)
        {
        case NoOp:
            return; // return - Don't change state

        case BeginDrag:

            //Begin drawing the selection rect
            if (fOperation == NoOp || fOperation == CancelSelection)
            {
                fSelectStartPoint = posWindowSpace;
                fOperation = operation;
            }

            //Begin Dragging or resizing the selection rect
            if (fOperation == EndDrag)
            {
                LLUtils::Corner corner = GetCorner(posWindowSpace);
                if (corner != LLUtils::Corner::None)
                {
                    //Selection rect already visible, re-drag
                    fOperation = BeginDrag;
                    fSelectStartPoint = fSelectionRect.GetCorner(OppositeCorner(corner));
                }
                else
                {
                    fSelectEndPoint = posWindowSpace;
                }
            }
            break;


        case Drag:
            {
                //Changing the size of the selection rect
                if (fOperation == BeginDrag || fOperation == Drag)
                {
                    using namespace LLUtils;
                    PointI32 p0 = {
                        std::min<PointI32::point_type>(posWindowSpace.x, fSelectStartPoint.x),
                        std::min<PointI32::point_type>(posWindowSpace.y, fSelectStartPoint.y)
                    };
                    PointI32 p1 = {
                        std::max<PointI32::point_type>(posWindowSpace.x, fSelectStartPoint.x),
                        std::max<PointI32::point_type>(posWindowSpace.y, fSelectStartPoint.y)
                    };
                    fSelectionRect = {p0,p1};
                    UpdateVisualSelectionRect();
                    fOperation = operation;
                }

                if (fOperation == EndDrag)
                {
                    using namespace LLUtils;
                    // Dragging the selection rect in the client window
                    fSelectionRect += posWindowSpace - fSelectEndPoint;
                    fSelectEndPoint = posWindowSpace;
                    UpdateVisualSelectionRect();
                }
            }
            break;
        case EndDrag:

            //Update operation state only when finshing setting the selection rect.
            if (fOperation == Drag)
                fOperation = operation;


            break;
        case CancelSelection:
            fOperation = operation;
            OIVCommands::CancelSelectionRect();
            break;
        }
    }
}
