#pragma once
namespace IMUtil
{
    enum class AxisAlignedRTransform
    {
          None
        , Rotate90CW
        , Rotate270CCW = Rotate90CW
        , Rotate90CCW
        , Rotate270CW = Rotate90CCW
        , Rotate180
        , FlipVertical
        , FlipHorizontal
    };
}
