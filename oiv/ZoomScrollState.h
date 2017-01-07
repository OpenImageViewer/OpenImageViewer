#pragma once
#include "Point.h"
namespace OIV
{

    class ZoomScrollState
    {
    public:
        class Listener
        {
        public:
            virtual LLUtils::PointI32 GetClientSize() = 0;
            virtual LLUtils::PointI32 GetImageSize() = 0;
            virtual void NotifyDirty() = 0;
        };

        ZoomScrollState(Listener* listener) :
            fLockSmallImagesToCenter(false)
            , fListener(listener)
            , fUVOffset(LLUtils::PointF64::Zero)
            , fUVScale(LLUtils::PointF64::One)
            , fSupressDirty(false)
            , fDirtyQueued(false)
            , fInverted(false)
            , fMarginLarge(0.25, 0.25)
            , fMarginSmall(0.1, 0.1)
        {


        }

        // commands
        void Reset(bool refresh = false);
        void Zoom(double amount,int x,int y);
        void Pan(LLUtils::PointF64 amont);
        void Refresh();


        // public queries
        LLUtils::PointF64 GetOffset() const { return fUVOffset; }
        LLUtils::PointF64 GetScale() const { return fUVScale; }
        LLUtils::PointF64 GetARFixedUVScale() const;
        LLUtils::PointF64 ClientPosToTexel(LLUtils::PointI32 pos) const;
        LLUtils::PointF64 GetNumTexelsInCanvas() const;

    private:
        // private queries
        LLUtils::PointF64 GetScreenSpaceOrigin() const;
        LLUtils::PointF64 GetImageSize() const;
        LLUtils::PointF64 GetClientSize() const;

        void SetInvertedVCoordinate(bool inverted);

    private: // methods
        void RefreshScale();
        void RefreshOffset();
        void NotifyDirty();


        void SetScale(LLUtils::PointF64 scale);
        void SupressDirty(bool surpress);

        void TranslateOffset(LLUtils::PointF64 offset);
        void Center();
        double ResolveOffset(double desiredOffset, double scale, double marginLarge, double marginSmall) const;
        LLUtils::PointF64 FixAR(LLUtils::PointF64 val, bool increase);
        void SetOffset(LLUtils::PointF64 offset);
        
    private: //member fields
        Listener* fListener;
        LLUtils::PointF64 fUVScale;
        LLUtils::PointF64 fUVOffset;
        bool fSupressDirty;
        bool fDirtyQueued;
        bool fInverted;
        bool fLockSmallImagesToCenter;

        LLUtils::PointF64 fMarginLarge;
        LLUtils::PointF64 fMarginSmall;

    };
}
