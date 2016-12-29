#include "ImageUtil.h"

namespace OIV
{

    ImageUtil::MapConvertKeyToFunc ImageUtil::sConvertionFunction
    {
        // Convert to RGBA
        {  ConvertKey(IT_BYTE_BGR,IT_BYTE_RGBA),PixelUtil::BGR24ToRGBA32 }
        ,{ ConvertKey(IT_BYTE_RGB ,IT_BYTE_RGBA),PixelUtil::RGB24ToRGBA32 }
        ,{ ConvertKey(IT_BYTE_BGRA,IT_BYTE_RGBA),PixelUtil::BGRA32ToRGBA32 }

    };
}
