#include "PreCompiled.h"
#include "ZoomScrollState.h"

namespace OIV
{
    void ZoomScrollState::TranslateOffset(Ogre::Vector2 offset)
    {
        SetOffset(fUVOffset + offset);
    }


    void ZoomScrollState::Center()
    {
        using namespace Ogre;
        Vector2 uvscale = GetARFixedUVScale();
        fUVOffset = -(uvscale - 1.0) / 2;
    }
    

    double ZoomScrollState::ResolveOffset(double desiredOffset, double scale, double marginLarge, double marginSmall) const
    {
        double offset;
        if (scale < 1)
        {
            // Image is larger than window
            double marginFactor = marginLarge * scale;
            double maxOffset = 1.0 - scale + marginFactor;
            offset = std::max(static_cast<double>(-marginFactor), std::min(maxOffset, desiredOffset));
        }
        else
        {

            // Image is smaller then window
            if (fLockSmallImagesToCenter)
            {
                //Keep in center
                offset = -(scale - 1) / 2;
            }
            else
            {
                double marginFactor = marginSmall * scale;
                double maxOffset = -marginFactor;
                double minOffset = -scale + 1.0 + marginFactor;
                if (minOffset < maxOffset)
                {
                    offset = std::min(std::max(desiredOffset, minOffset), maxOffset);
                }
                else
                {
                    //No room for margin - Keep in center
                    offset = -(scale - 1) / 2;
                }
            }
        }

        return offset;
    }

    void ZoomScrollState::SetOffset(Ogre::Vector2 offset)
    {
        Ogre::Vector2 uvScale = GetARFixedUVScale();
        fUVOffset.x = ResolveOffset(offset.x, uvScale.x, fMarginLarge.x , fMarginSmall.x);
        fUVOffset.y = ResolveOffset(offset.y, uvScale.y, fMarginLarge.y, fMarginSmall.y);
        NotifyDirty();
    }


    Ogre::Vector2 ZoomScrollState::GetScreenSpaceOrigin()
    {
        return GetOffset() / GetARFixedUVScale() * fListener->GetWindowSize();
    }

    Ogre::Vector2 ZoomScrollState::ClientPosToTexel(Ogre::Vector2 pos)
    {
        using namespace  Ogre;
        Vector2 texel = (pos + GetScreenSpaceOrigin()) / fListener->GetWindowSize() * fListener->GetImageSize() * GetARFixedUVScale();
        return texel;
    }

    void ZoomScrollState::SetScale(Ogre::Vector2 scale)
    {
        fUVScale = scale;
        NotifyDirty();
    }

    Ogre::Vector2 ZoomScrollState::GetCanvasSize()
    {
        return fListener->GetImageSize() * GetARFixedUVScale();
    }

    Ogre::Vector2 ZoomScrollState::GetARFixedUVScale()
    {
        using namespace Ogre;
        
        Vector2 windowSize = fListener->GetWindowSize();
        Vector2 textureSize = fListener->GetImageSize();

        Real textureAR = textureSize.x / textureSize.y;
        Real windowAR = windowSize.x / windowSize.y;


        Vector2 fixedUVScale = fUVScale;
        Real ARFix = textureAR / windowAR;;
        if (ARFix < 1)
            fixedUVScale.x /= ARFix;
        else
            fixedUVScale.y *= ARFix;

        return fixedUVScale;
    }

    void ZoomScrollState::Refresh()
    {
        SupressDirty(true);
        RefreshScale();
        RefreshOffset();
        SupressDirty(false);

    }

    void ZoomScrollState::RefreshScale()
    {
        SetScale(fUVScale);
    }

    void ZoomScrollState::RefreshOffset()
    {
        SetOffset(fUVOffset);
    }

    void ZoomScrollState::NotifyDirty()
    {
        if (fSupressDirty == false)
            fListener->NotifyDirty();
        else
            fDirtyQueued = true;
    }

    void ZoomScrollState::SupressDirty(bool surpress)
    {
        if (surpress == true)
        {
            fSupressDirty = true;
        }
        else
        {
            fSupressDirty = false;
            if (fDirtyQueued)
            {
                NotifyDirty();
                fDirtyQueued = false;
            }
        }
    }

    void ZoomScrollState::Reset(bool refresh /*= false*/)
    {
        fUVOffset = Ogre::Vector2::ZERO;
        fUVScale = Ogre::Vector2::UNIT_SCALE;

        Center();

        if (refresh)
            Refresh();
    }

    void ZoomScrollState::Zoom(Ogre::Real amount)
    {
        if (amount == 0)
            return;

        SupressDirty(true);
        using namespace Ogre;

        Vector2 mousePos = fListener->GetMousePosition();
        Vector2 windowSize = fListener->GetWindowSize();
        Vector2 screenOffset = (mousePos / windowSize);
        if (fInverted)
        {
            assert(screenOffset.y <= 1.0);
            screenOffset.y = 1 - screenOffset.y;
        }


        Real currentZoom = 1 / GetScale().x;
        
        Real targetZoom;

        if (amount > 0)
            targetZoom = currentZoom * (1 + amount);
        else
        if (amount < 0)
            targetZoom = currentZoom *( 1.0 - std::min(-amount,0.95f));

        Vector2 uvNewScale = Vector2::UNIT_SCALE /  targetZoom;

        Vector2 oldScaleFixed = GetARFixedUVScale();
        SetScale(uvNewScale);
        Vector2 newScaleFixed = GetARFixedUVScale();

        Vector2 totalOffset = (oldScaleFixed - newScaleFixed);

        //zoom relative to mouse position
        Vector2 offsetChange = totalOffset * screenOffset;
        TranslateOffset(offsetChange);
        SupressDirty(false);
    }

    void ZoomScrollState::SetInvertedVCoordinate(bool inverted)
    {
        fInverted = inverted;
    }

    void ZoomScrollState::Pan(Ogre::Vector2 amount)
    {
        Ogre::Vector2 panFactor = amount * GetARFixedUVScale() / fListener->GetWindowSize();
        panFactor.y *= fInverted ? -1 : 1;
        TranslateOffset(panFactor);
    }
}