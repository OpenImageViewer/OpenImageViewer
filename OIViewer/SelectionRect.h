#pragma once
#include "OIVCommands.h"

namespace OIV
{
    class SelectionRect
    {

    public:
        using SelectionRectChangedCallback = std::function<void(const LLUtils::RectI32&, bool)>;
        enum class Operation
        {
              NoOp
            , BeginDrag
            , Drag
            , EndDrag
            , CancelSelection
        };

        enum class LockMode
        {
              NoLock
            , LockWidth
            , LockHeight
        };

        SelectionRect(SelectionRectChangedCallback callback);
        void SetSelection(const Operation operation, const LLUtils::PointI32& posWindowSpace);
        void UpdateSelection(const LLUtils::RectI32& selectionRect);
        const LLUtils::RectI32& GetSelectionRect() const;
        Operation GetOperation() const { return fOperation; }

    private: // methods
        void NotifySelectionRectChanged(bool isVisible) const;
        LLUtils::Corner OppositeCorner(const LLUtils::Corner corner) const;
        LLUtils::Corner GetClosestCorner(const LLUtils::PointI32& point) const;


    private: // member fields
        Operation fOperation = Operation::NoOp;
        LLUtils::RectI32 fSelectionRect;
        LLUtils::PointI32 fSelectStartPoint;
        LLUtils::PointI32 fSelectEndPoint;
        const uint16_t pixelsThreshold = 15;
        const uint16_t pixelsThresholdSquare = pixelsThreshold * pixelsThreshold;
        LockMode fLockMode = LockMode::NoLock;
        SelectionRectChangedCallback fCallback;
    };
}
