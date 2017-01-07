#include <algorithm>
#include <cassert>
#include "ZoomScrollState.h"


namespace OIV
{
    void ZoomScrollState::TranslateOffset(LLUtils::PointF64 offset)
    {
        SetOffset(fUVOffset + offset);
    }


    void ZoomScrollState::Center()
    {
        LLUtils::PointF64 uvscale = GetARFixedUVScale();
        fUVOffset = -(uvscale - 1.0) * 0.5;
    }
    

    double ZoomScrollState::ResolveOffset(double desiredOffset, double scale, double marginLarge, double marginSmall) const
    {
        double offset;
        if (scale < 1)
        {
            // Image is larger than window
            double marginFactor = marginLarge * scale;
            double maxOffset = 1.0 - scale + marginFactor;
            offset = (std::max)(static_cast<double>(-marginFactor), (std::min)(maxOffset, desiredOffset));
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
                    offset = (std::min)((std::max)(desiredOffset, minOffset), maxOffset);
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

    LLUtils::PointF64 ZoomScrollState::FixAR(LLUtils::PointF64 val, bool increase)
    {
        LLUtils::PointF64 windowSize = GetClientSize();
        LLUtils::PointF64 result;
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

    void ZoomScrollState::SetOffset(LLUtils::PointF64 offset)
    {
        LLUtils::PointF64 uvScale = GetARFixedUVScale();

        LLUtils::PointF64 marginLarge = FixAR(fMarginLarge, false);
        LLUtils::PointF64 marginSmall = FixAR(fMarginSmall, false);
        
        
        fUVOffset.x = ResolveOffset(offset.x, uvScale.x, marginLarge.x, marginSmall.x);
        fUVOffset.y = ResolveOffset(offset.y, uvScale.y, marginLarge.y, marginSmall.y);
        NotifyDirty();
    }


    LLUtils::PointF64 ZoomScrollState::GetScreenSpaceOrigin() const
    {
        return GetOffset() / GetARFixedUVScale() * static_cast<LLUtils::PointF64>( fListener->GetClientSize());
    }

    LLUtils::PointF64 ZoomScrollState::GetImageSize() const
    {
        return static_cast<LLUtils::PointF64>(fListener->GetImageSize());
    }

    LLUtils::PointF64 ZoomScrollState::GetClientSize() const
    {
        return static_cast<LLUtils::PointF64>(fListener->GetClientSize());
    }


    LLUtils::PointF64 ZoomScrollState::ClientPosToTexel(LLUtils::PointI32 pos) const
    {
        LLUtils::PointF64 texel = (static_cast<LLUtils::PointF64>(pos) + GetScreenSpaceOrigin()) / GetClientSize() * GetImageSize() * GetARFixedUVScale();
        return texel;
    }

    void ZoomScrollState::SetScale(LLUtils::PointF64 scale)
    {
        fUVScale = scale;
        NotifyDirty();
    }

    LLUtils::PointF64 ZoomScrollState::GetNumTexelsInCanvas() const
    {
        return GetImageSize() * GetARFixedUVScale();
    }

    LLUtils::PointF64 ZoomScrollState::GetARFixedUVScale() const
    {
        LLUtils::PointF64 windowSize = GetClientSize();
        LLUtils::PointF64 textureSize = GetImageSize();
        if (textureSize == LLUtils::PointF64::Zero)
            return fUVScale;

        double textureAR = textureSize.x / textureSize.y;
        double windowAR = windowSize.x / windowSize.y;


        LLUtils::PointF64 fixedUVScale = fUVScale;
        double ARFix = textureAR / windowAR;;
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
        fUVOffset = LLUtils::PointF64::Zero;
        fUVScale = LLUtils::PointF64::One;

        Center();

        if (refresh)
            Refresh();
    }

    void ZoomScrollState::Zoom(double amount, int x, int y)
    {
        using namespace LLUtils;
        if (amount == 0)
            return;

        SupressDirty(true);

        PointF64 zoomPoint;
        PointF64 windowSize = GetClientSize();

        
        zoomPoint.x = x >= 0 ? (static_cast<double>(x) / windowSize.x) : 0.5;
        zoomPoint.y = y >= 0 ? (static_cast<double>(y) / windowSize.y) : 0.5;

        
        if (fInverted)
        {
            assert(zoomPoint.y <= 1.0);
            zoomPoint.y = 1 - zoomPoint.y;
        }


        double currentZoom = 1 / GetScale().x;
        
        double targetZoom;

        if (amount > 0)
            targetZoom = currentZoom * (1 + amount);
        else
        if (amount < 0)
            targetZoom = currentZoom *( 1.0 - (std::min)(-amount,0.95));

        PointF64 uvNewScale = PointF64::One /  targetZoom;

        PointF64 oldScaleFixed = GetARFixedUVScale();
        SetScale(uvNewScale);
        PointF64 newScaleFixed = GetARFixedUVScale();

        PointF64 totalOffset = (oldScaleFixed - newScaleFixed);

        //zoom around the zoom point
        PointF64 offsetChange = totalOffset * zoomPoint;
        TranslateOffset(offsetChange);
        SupressDirty(false);
    }

    void ZoomScrollState::SetInvertedVCoordinate(bool inverted)
    {
        fInverted = inverted;
    }

    void ZoomScrollState::Pan(LLUtils::PointF64 amount)
    {
        LLUtils::PointF64 panFactor = amount * GetARFixedUVScale() / GetClientSize();
        panFactor.y *= fInverted ? -1 : 1;
        TranslateOffset(panFactor);
    }
}
