#pragma once
namespace IMUtil
{
    enum AxisAlignedRTransform
    {
          AAT_None
        , AAT_Rotate90CW
        , AAT_Rotate270CCW = AAT_Rotate90CW
        , AAT_Rotate90CCW
        , AAT_Rotate270CW = AAT_Rotate90CCW
        , AAT_Rotate180
        , AAT_FlipVertical
        , AAT_FlipHorizontal
    };
}
