#pragma once

#include <oivappcore/SelectionRect.h>

#include <LLUtils/Point.h>
#include <LLUtils/Rect.h>

#include <string>

namespace OIV
{
    class SelectionWorkflowPolicy
    {
      public:
        static std::wstring FormatSelectionSize(const LLUtils::RectI32& imageSpaceSelection);
        static LLUtils::PointI32 PlaceSelectionLabel(const LLUtils::RectI32& selectionRect,
                                                     LLUtils::PointI32 labelSize,
                                                     LLUtils::PointI32 clientSize);
        static LLUtils::PointI32 SnapToImagePixels(LLUtils::PointI32 clientPoint,
                                                   double scale,
                                                   LLUtils::PointF64 offset);
        static bool ShouldSaveImageSpaceSelection(SelectionRect::Operation operation);
    };
}
