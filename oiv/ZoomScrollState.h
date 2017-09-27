#pragma once
#include "Point.h"
#include "API/defs.h"

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
            
             fListener(listener)
            
        {

        }

        // commands
        void SetZoom(double zoom);
        void SetOffset(LLUtils::PointF64 offset);
        void Refresh();


        // public queries
        LLUtils::PointF64 GetOffset() const { return fUVOffset; }
        LLUtils::PointF64 GetScale() const { return fUVScale; }
        LLUtils::PointF64 GetARFixedUVScale() const;
        LLUtils::PointF64 ClientPosToTexel(LLUtils::PointI32 pos) const;
        LLUtils::PointF64 GetNumTexelsInCanvas() const;
        LLUtils::PointF64 GetRealSizeScale() const;
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
        
        LLUtils::PointF64 FixAR(LLUtils::PointF64 val, bool increase);
        
    private: //member fields
        Listener* fListener = nullptr;
        LLUtils::PointF64 fUVScale = LLUtils::PointF64::One;
        LLUtils::PointF64 fUVOffset = LLUtils::PointF64::Zero;
        bool fSupressDirty = false;
        bool fDirtyQueued = false;
        bool fInverted = false;
     
        double fZoom = 1.0;
        LLUtils::PointF64 fOffset = LLUtils::PointF64::Zero;
    };
}
