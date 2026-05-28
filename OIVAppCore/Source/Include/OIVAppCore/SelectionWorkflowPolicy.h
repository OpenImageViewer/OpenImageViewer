#pragma once

#include <LLUtils/StringDefs.h>

#include <OIVAppCore/SelectionRect.h>

#include <LLUtils/Point.h>
#include <LLUtils/Rect.h>

#include <string>

namespace OIV
{
    class SelectionWorkflowPolicy
    {
      public:

        static LLUtils::native_string_type FormatSelectionSize(const LLUtils::RectI32& imageSpaceSelection);
        static LLUtils::PointI32 PlaceSelectionLabel(const LLUtils::RectI32& selectionRect, LLUtils::PointI32 labelSize,
                                                     LLUtils::PointI32 clientSize);
        static LLUtils::PointI32 SnapToImagePixels(LLUtils::PointI32 clientPoint, double scale,
                                                   LLUtils::PointF64 offset);
        static bool ShouldSaveImageSpaceSelection(SelectionRect::Operation operation);
    };
}  // namespace OIV
