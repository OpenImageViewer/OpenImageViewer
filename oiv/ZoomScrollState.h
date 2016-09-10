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
            virtual Ogre::Vector2 GetMousePosition() = 0;
            virtual Ogre::Vector2 GetWindowSize() = 0;
            virtual Ogre::Vector2 GetImageSize() = 0;
            virtual void NotifyDirty() = 0;
        };

        ZoomScrollState(Listener* listener)
        {
            fListener = listener;
            fUVOffset = Ogre::Vector2::ZERO;
            fUVScale = Ogre::Vector2::UNIT_SCALE;
            fSupressDirty = false;
            fDirtyQueued = false;
        }

        void Zoom(Ogre::Real amount);
        void Pan(Ogre::Vector2 amont);

        void NotifyDirty();

        Ogre::Vector2 GetOffset() { return fUVOffset; }
        Ogre::Vector2 GetScale() { return fUVScale; }
        void SetScale(Ogre::Vector2 scale);
        void SupressDirty(bool surpress);

        void TranslateOffset(Ogre::Vector2 offset);
        void SetOffset(Ogre::Vector2 offset);
        Ogre::Vector2 GetARFixedUVScale();
        
        void RefreshScale();
        void RefreshOffset();


    private:
        Listener* fListener;
        Ogre::Vector2 fUVScale;
        Ogre::Vector2 fUVOffset;
        bool fSupressDirty;
        bool fDirtyQueued;
    };
}
