#include "ImageUtil.h"

namespace IMUtil
{

    ImageUtil::MapConvertKeyToFunc ImageUtil::sConvertionFunction
    {
        // Convert to RGBA
        {  ConvertKey(IMCodec::IT_BYTE_BGR, IMCodec::IT_BYTE_RGBA),PixelUtil::BGR24ToRGBA32 }
        ,{ ConvertKey(IMCodec::IT_BYTE_RGB , IMCodec::IT_BYTE_RGBA),PixelUtil::RGB24ToRGBA32 }
        ,{ ConvertKey(IMCodec::IT_BYTE_BGRA, IMCodec::IT_BYTE_RGBA),PixelUtil::BGRA32ToRGBA32 }

    };
}
