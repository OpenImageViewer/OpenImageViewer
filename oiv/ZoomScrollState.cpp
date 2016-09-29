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
    

    void ZoomScrollState::SetOffset(Ogre::Vector2 offset)
    {
        using namespace Ogre;
        Vector2 Max_Margin = Vector2(0.25, 0.25);
        const bool KeepSmallerImagesInCenter = false;
        
        Vector2 uvScale = GetARFixedUVScale();

        const Vector2 marginFactor = Vector2(Max_Margin.x * uvScale.x, Max_Margin.y * uvScale.y);
        // Image is visually larger then window
        if (uvScale.x < 1)
        {
            
            double maxOffset = 1.0 - uvScale.x + marginFactor.x;
            fUVOffset.x = std::max(static_cast<double>(-marginFactor.x), std::min(maxOffset, (double)offset.x));
        }
        else
        {
            Max_Margin = Vector2(0.1, 0.1);
            // Image is smaller then window
            if (KeepSmallerImagesInCenter)
            {
                //Keep in center
                fUVOffset.x = -(uvScale.x - 1) / 2;// offset.x;
            }
            else
            {
                double marginFactor = Max_Margin.x * uvScale.x;
                double maxOffset = -marginFactor;
                double minOffset = -uvScale.x + 1.0 + marginFactor;
                if (minOffset < maxOffset)
                {
                    fUVOffset.x = std::min(std::max((double)offset.x, minOffset), maxOffset);
                }
                else
                {
                    //No room for margin - Keep in center
                    fUVOffset.x = -(uvScale.x - 1) / 2;// offset.x;
                }
            }
        }

        if (uvScale.y < 1)
        {
            double maxOffset = 1.0 - uvScale.y + marginFactor.y;
            fUVOffset.y = std::max(static_cast<double>(-marginFactor.y), std::min(maxOffset, (double)offset.y));
        }
        else
        {

            Max_Margin = Vector2(0.05, 0.05);
            if (KeepSmallerImagesInCenter)
            {
                //Keep in center
                fUVOffset.y = -(uvScale.y - 1) / 2;// offset.x;
            }
            else
            {
                double marginFactor = Max_Margin.y * uvScale.y;
                double maxOffset = -marginFactor;
                double minOffset = -uvScale.y + 1.0 + marginFactor;
                if (minOffset < maxOffset)
                {
                    fUVOffset.y = std::min(std::max((double)offset.y, minOffset), maxOffset);
                }
                else
                {
                    //No room for margin - Keep in center
                    fUVOffset.y = -(uvScale.y - 1) / 2;// offset.x;
                }
            }
        }
        NotifyDirty();
    }

    void ZoomScrollState::SetScale(Ogre::Vector2 scale)
    {
        fUVScale = scale;
        NotifyDirty();
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