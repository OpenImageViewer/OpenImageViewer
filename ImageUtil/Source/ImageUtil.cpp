#include "ImageUtil.h"

namespace IMUtil
{

    ImageUtil::MapConvertKeyToFunc ImageUtil::sConvertionFunction
    {
        // Convert to RGBA
        {  ConvertKey(IMCodec::TF_I_B8_G8_R8,    IMCodec::TF_I_R8_G8_B8_A8),PixelUtil::BGR24ToRGBA32 }
        ,{ ConvertKey(IMCodec::TF_I_R8_G8_B8,    IMCodec::TF_I_R8_G8_B8_A8),PixelUtil::RGB24ToRGBA32 }
        ,{ ConvertKey(IMCodec::TF_I_B8_G8_R8_A8, IMCodec::TF_I_R8_G8_B8_A8),PixelUtil::BGRA32ToRGBA32 }

    };
}
