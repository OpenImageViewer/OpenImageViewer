#pragma once
#include <LLUtils/EnumClassBitwise.h>

namespace IMUtil
{
    enum OIV_AxisAlignedRotation
    {
          None          = 0
        , Rotate90CW    = 1
        , Rotate180     = 2
        , Rotate90CCW   = 3
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

LLUTILS_DEFINE_ENUM_CLASS_FLAG_OPERATIONS(IMUtil::OIV_AxisAlignedFlip)
