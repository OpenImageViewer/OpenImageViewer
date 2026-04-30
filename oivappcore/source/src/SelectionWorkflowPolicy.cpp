#include <oivappcore/SelectionWorkflowPolicy.h>

#include <oivshared/ViewTransformController.h>

#include <algorithm>

namespace OIV
{
    std::wstring SelectionWorkflowPolicy::FormatSelectionSize(const LLUtils::RectI32& imageSpaceSelection)
    {
        return std::to_wstring(imageSpaceSelection.GetWidth()) + L" X " +
               std::to_wstring(imageSpaceSelection.GetHeight());
    }

    LLUtils::PointI32 SelectionWorkflowPolicy::PlaceSelectionLabel(const LLUtils::RectI32& selectionRect,
                                                                   LLUtils::PointI32 labelSize,
                                                                   LLUtils::PointI32 clientSize)
    {
        const auto topLeft = selectionRect.GetCorner(LLUtils::Corner::TopLeft);
        const auto bottomRight = selectionRect.GetCorner(LLUtils::Corner::BottomRight);

        int32_t posX = topLeft.x + selectionRect.GetWidth() / 2 - labelSize.x / 2;
        int32_t posY = topLeft.y - labelSize.y;

        if (posY < 0)
            posY = bottomRight.y;

        if (posY + labelSize.y >= clientSize.y)
            posY = std::max(0, topLeft.y);

        if (posX + labelSize.x >= clientSize.x)
        {
            posX = topLeft.x - labelSize.x;
            posY = topLeft.y + (bottomRight.y - topLeft.y) / 2;
        }

        if (posX < 0)
        {
            posX = bottomRight.x;
            posY = topLeft.y + (bottomRight.y - topLeft.y) / 2;
        }

        return {posX, posY};
    }

    LLUtils::PointI32 SelectionWorkflowPolicy::SnapToImagePixels(LLUtils::PointI32 clientPoint,
                                                                 double scale,
                                                                 LLUtils::PointF64 offset)
    {
        const auto imagePoint = static_cast<LLUtils::PointI32>(
            ViewTransformController::ClientToImage(static_cast<LLUtils::PointF64>(clientPoint), scale, offset).Round());
        const auto snappedClientPoint =
            ViewTransformController::ImageToClient(static_cast<LLUtils::PointF64>(imagePoint), scale, offset);
        return static_cast<LLUtils::PointI32>(snappedClientPoint.Round());
    }

    bool SelectionWorkflowPolicy::ShouldSaveImageSpaceSelection(SelectionRect::Operation operation)
    {
        return operation != SelectionRect::Operation::NoOp;
    }
}
