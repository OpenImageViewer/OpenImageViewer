#pragma once
#include <LLUtils/Rect.h>

#include <functional>
#include <optional>

namespace OIV
{
    class SelectionRect
    {
      public:

        using SelectionRectChangedCallback = std::function<void(const LLUtils::RectI32&, bool)>;
        enum class Operation
        {
            NoOp,
            BeginDrag,
            Drag,
            EndDrag,
            CancelSelection
        };

        enum class Handle
        {
            None,
            Move,
            TopLeft,
            TopRight,
            BottomLeft,
            BottomRight,
            Top,
            Bottom,
            Left,
            Right
        };
        void SetContentScale(LLUtils::PointF64 scale);
        Handle HitTest(const LLUtils::PointI32& point) const;

        enum class LockMode
        {
            NoLock,
            LockWidth,
            LockHeight
        };

        SelectionRect(SelectionRectChangedCallback callback);
        // BeginDrag can use an unsnapped hover result while retaining a pixel-snapped anchor.
        void SetSelection(const Operation operation, const LLUtils::PointI32& posWindowSpace,
                          std::optional<Handle> hitHandle = std::nullopt);
        void UpdateSelection(const LLUtils::RectI32& selectionRect);
        const LLUtils::RectI32& GetSelectionRect() const;
        Operation GetOperation() const { return fOperation; }

      private:  // methods

        void NotifySelectionRectChanged(bool isVisible) const;

      private:  // member fields

        static constexpr int EdgeTolerance   = 6;
        static constexpr int CornerTolerance = 10;

        Operation fOperation = Operation::NoOp;
        LLUtils::RectI32 fSelectionRect{};
        LLUtils::PointI32 fSelectStartPoint{};
        LLUtils::PointI32 fSelectEndPoint{};
        LLUtils::PointI32 fEdgeTolerance{EdgeTolerance, EdgeTolerance};
        LLUtils::PointI32 fCornerTolerance{CornerTolerance, CornerTolerance};
        LockMode fLockMode = LockMode::NoLock;
        SelectionRectChangedCallback fCallback;
    };
}  // namespace OIV
