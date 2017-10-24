#pragma once
#include "OIVCommands.h"

namespace OIV
{
    class SelectionRect
    {

    public:

        enum Operation
        {
              NoOp
            , BeginDrag
            , Drag
            , EndDrag
            , CancelSelection
        };

        void SetSelection(const Operation operation, const LLUtils::PointI32& posWindowSpace);
        const LLUtils::RectI32& GetSelectionRect() const;

    private: // methods
        void UpdateVisualSelectionRect() const;
        LLUtils::Corner OppositeCorner(const LLUtils::Corner corner) const;
        LLUtils::Corner GetCorner(const LLUtils::PointI32& point) const;


    private: // member fields
        Operation fOperation = NoOp;
        LLUtils::RectI32 fSelectionRect;
        LLUtils::PointI32 fSelectStartPoint;
        LLUtils::PointI32 fSelectEndPoint;
        const bool pixelsThreshold = 15;
        const bool pixelsThresholdSquare = pixelsThreshold * pixelsThreshold;
    };
}
