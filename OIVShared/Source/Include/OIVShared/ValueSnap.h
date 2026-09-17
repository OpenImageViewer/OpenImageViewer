#pragma once

namespace OIV
{
    // Zoom and playback speed use 1.0 for their original value. Snap movement toward it when
    // the requested value enters 97-103% or crosses 100%, even if it overshoots that range.
    // Starting at 100%, moving away, and no movement bypass the snap. Input acceleration
    // belongs to the caller and is preserved so the next event can leave the snapped value.
    constexpr double SnapToOriginalValue(double currentValue, double requestedValue)
    {
        constexpr double OriginalValue = 1.0;
        constexpr double SnapTolerance = 0.03;
        const bool snapFromBelow       = currentValue < OriginalValue && requestedValue > currentValue &&
                                         requestedValue >= OriginalValue - SnapTolerance;
        const bool snapFromAbove       = currentValue > OriginalValue && requestedValue < currentValue &&
                                         requestedValue <= OriginalValue + SnapTolerance;
        return snapFromBelow || snapFromAbove ? OriginalValue : requestedValue;
    }
}  // namespace OIV
