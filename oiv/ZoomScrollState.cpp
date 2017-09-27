#include <algorithm>
#include <cassert>
#include "ZoomScrollState.h"


namespace OIV
{

    LLUtils::PointF64 ZoomScrollState::GetImageSize() const
    {
        return static_cast<LLUtils::PointF64>(fListener->GetImageSize());
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

    

    LLUtils::PointF64 ZoomScrollState::GetScreenSpaceOrigin() const
    {
        return GetOffset() / GetARFixedUVScale() * static_cast<LLUtils::PointF64>( fListener->GetClientSize());
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

    LLUtils::PointF64 ZoomScrollState::GetRealSizeScale() const
    {
        LLUtils::PointF64 ratio = GetClientSize() / GetImageSize();
        return LLUtils::PointF64(std::min(ratio.x, ratio.y));
    }

   

    LLUtils::PointF64 ZoomScrollState::GetARFixedUVScale() const
    {
        LLUtils::PointF64 windowSize = GetClientSize();
        LLUtils::PointF64 textureSize = GetImageSize();
        if (textureSize == LLUtils::PointF64::Zero)
            return fUVScale;

        double textureAR = textureSize.x / std::max(textureSize.y, 0.1);
        double windowAR = windowSize.x / std::max(windowSize.y, 0.1);


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
        fUVScale = GetRealSizeScale() / fZoom;
    }

    void ZoomScrollState::RefreshOffset()
    {
        using namespace LLUtils;
        //Center();
        fUVOffset = -fOffset  / GetClientSize() * GetARFixedUVScale();
        NotifyDirty();
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


    void ZoomScrollState::SetZoom(double amount)
    {
        using namespace LLUtils;
        if (amount == 0)
            return;

        SupressDirty(true);
        fZoom = amount;
        RefreshScale();
        RefreshOffset();
        SupressDirty(false);
    }

    void ZoomScrollState::SetInvertedVCoordinate(bool inverted)
    {
        fInverted = inverted;
    }

    void ZoomScrollState::SetOffset(LLUtils::PointF64 amount)
    {
        fOffset = amount;
        RefreshOffset();
        NotifyDirty();
    }
}