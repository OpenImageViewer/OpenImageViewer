#pragma once
#include "OgreVector2.h"
namespace OIV
{

    class ZoomScrollState
    {
    public:
        class Listener
        {
        public:
            virtual Ogre::Vector2 GetClientSize() = 0;
            virtual Ogre::Vector2 GetImageSize() = 0;
            virtual void NotifyDirty() = 0;
        };

        ZoomScrollState(Listener* listener) :
            fLockSmallImagesToCenter(false)
            , fListener(listener)
            , fUVOffset(Ogre::Vector2::ZERO)
            , fUVScale(Ogre::Vector2::UNIT_SCALE)
            , fSupressDirty(false)
            , fDirtyQueued(false)
            , fInverted(false)
            , fMarginLarge(0.25, 0.25)
            , fMarginSmall(0.1, 0.1)
        {


        }

        // commands
        void Reset(bool refresh = false);
        void Zoom(Ogre::Real amount,int x,int y);
        void Pan(Ogre::Vector2 amont);
        void Refresh();


        // queries
        Ogre::Vector2 GetOffset() { return fUVOffset; }
        Ogre::Vector2 GetScale() { return fUVScale; }
        Ogre::Vector2 GetARFixedUVScale();
        Ogre::Vector2 ClientPosToTexel(Ogre::Vector2 pos);
        Ogre::Vector2 GetNumTexelsInCanvas();

        void SetInvertedVCoordinate(bool inverted);

    private: // methods
        void RefreshScale();
        void RefreshOffset();
        void NotifyDirty();


        void SetScale(Ogre::Vector2 scale);
        void SupressDirty(bool surpress);

        void TranslateOffset(Ogre::Vector2 offset);
        void Center();
        double ResolveOffset(double desiredOffset, double scale, double marginLarge, double marginSmall) const;
        Ogre::Vector2 FixAR(Ogre::Vector2 val, bool increase);
        void SetOffset(Ogre::Vector2 offset);
        Ogre::Vector2 GetScreenSpaceOrigin();

    private: //member fields
        Listener* fListener;
        Ogre::Vector2 fUVScale;
        Ogre::Vector2 fUVOffset;
        bool fSupressDirty;
        bool fDirtyQueued;
        bool fInverted;
        bool fLockSmallImagesToCenter;

        Ogre::Vector2 fMarginLarge;
        Ogre::Vector2 fMarginSmall;

    };
}
