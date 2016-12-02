#pragma once
#include "Vector2.h"
namespace OIV
{

    class ZoomScrollState
    {
    public:
        class Listener
        {
        public:
            virtual Vector2 GetClientSize() = 0;
            virtual Vector2 GetImageSize() = 0;
            virtual void NotifyDirty() = 0;
        };

        ZoomScrollState(Listener* listener) :
            fLockSmallImagesToCenter(false)
            , fListener(listener)
            , fUVOffset(Vector2::ZERO)
            , fUVScale(Vector2::UNIT_SCALE)
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
        void Pan(Vector2 amont);
        void Refresh();


        // queries
        Vector2 GetOffset() { return fUVOffset; }
        Vector2 GetScale() { return fUVScale; }
        Vector2 GetARFixedUVScale();
        Vector2 ClientPosToTexel(Vector2 pos);
        Vector2 GetNumTexelsInCanvas();

        void SetInvertedVCoordinate(bool inverted);

    private: // methods
        void RefreshScale();
        void RefreshOffset();
        void NotifyDirty();


        void SetScale(Vector2 scale);
        void SupressDirty(bool surpress);

        void TranslateOffset(Vector2 offset);
        void Center();
        double ResolveOffset(double desiredOffset, double scale, double marginLarge, double marginSmall) const;
        Vector2 FixAR(Vector2 val, bool increase);
        void SetOffset(Vector2 offset);
        Vector2 GetScreenSpaceOrigin();

    private: //member fields
        Listener* fListener;
        Vector2 fUVScale;
        Vector2 fUVOffset;
        bool fSupressDirty;
        bool fDirtyQueued;
        bool fInverted;
        bool fLockSmallImagesToCenter;

        Vector2 fMarginLarge;
        Vector2 fMarginSmall;

    };
}
