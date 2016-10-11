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

    Ogre::Vector2 ZoomScrollState::FixAR(Ogre::Vector2 val, bool increase)
    {
        using namespace Ogre;

        Vector2 windowSize = fListener->GetClientSize();
        Vector2 result;
        double ar = windowSize.x / windowSize.y;
        result = val;

        if (ar < 1)
        {
            if (increase)
                result.x /= ar;
            else
                result.y *= ar;
        }

        else if (ar > 1)
        {
            if (increase)
                result.y *= ar;
            else
                result.x /= ar;
        }
        return result;
    }

    void ZoomScrollState::SetOffset(Ogre::Vector2 offset)
    {
        using namespace Ogre;
        Vector2 uvScale = GetARFixedUVScale();

        Vector2 marginLarge = FixAR(fMarginLarge, false);
        Vector2 marginSmall = FixAR(fMarginSmall, false);
        
        
        fUVOffset.x = ResolveOffset(offset.x, uvScale.x, marginLarge.x, marginSmall.x);
        fUVOffset.y = ResolveOffset(offset.y, uvScale.y, marginLarge.y, marginSmall.y);
        NotifyDirty();
    }


    Ogre::Vector2 ZoomScrollState::GetScreenSpaceOrigin()
    {
        return GetOffset() / GetARFixedUVScale() * fListener->GetClientSize();
    }

    Ogre::Vector2 ZoomScrollState::ClientPosToTexel(Ogre::Vector2 pos)
    {
        using namespace  Ogre;
        Vector2 texel = (pos + GetScreenSpaceOrigin()) / fListener->GetClientSize() * fListener->GetImageSize() * GetARFixedUVScale();
        return texel;
    }

    void ZoomScrollState::SetScale(Ogre::Vector2 scale)
    {
        fUVScale = scale;
        NotifyDirty();
    }

    Ogre::Vector2 ZoomScrollState::GetNumTexelsInCanvas()
    {
        return fListener->GetImageSize() * GetARFixedUVScale();
    }

    Ogre::Vector2 ZoomScrollState::GetARFixedUVScale()
    {
        using namespace Ogre;
        
        Vector2 windowSize = fListener->GetClientSize();
        Vector2 textureSize = fListener->GetImageSize();
        if (textureSize == Vector2::ZERO)
            return fUVScale;

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

    void ZoomScrollState::Zoom(Ogre::Real amount, int x, int y)
    {
        if (amount == 0)
            return;

        SupressDirty(true);
        using namespace Ogre;

        Vector2 zoomPoint;
        Vector2 windowSize = fListener->GetClientSize();

        
        zoomPoint.x = x >= 0 ? (static_cast<double>(x) / windowSize.x) : 0.5;
        zoomPoint.y = y >= 0 ? (static_cast<double>(y) / windowSize.y) : 0.5;

        
        if (fInverted)
        {
            assert(zoomPoint.y <= 1.0);
            zoomPoint.y = 1 - zoomPoint.y;
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

        //zoom around the zoom point
        Vector2 offsetChange = totalOffset * zoomPoint;
        TranslateOffset(offsetChange);
        SupressDirty(false);
    }

    void ZoomScrollState::SetInvertedVCoordinate(bool inverted)
    {
        fInverted = inverted;
    }

    void ZoomScrollState::Pan(Ogre::Vector2 amount)
    {
        Ogre::Vector2 panFactor = amount * GetARFixedUVScale() / fListener->GetClientSize();
        panFactor.y *= fInverted ? -1 : 1;
        TranslateOffset(panFactor);
    }
}