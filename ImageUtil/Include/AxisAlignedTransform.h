#pragma once
namespace IMUtil
{
    enum OIV_AxisAlignedRotation
    {
          None
        , Rotate90CW
        , Rotate180
        , Rotate90CCW
    };

    enum class OIV_AxisAlignedFlip
    {
          None          = 0 << 0
        , Horizontal    = 1 << 0
        , Vertical      = 1 << 1
    };


    struct OIV_AxisAlignedTransform
    {
        OIV_AxisAlignedRotation rotation;
        OIV_AxisAlignedFlip flip;
    };
}
