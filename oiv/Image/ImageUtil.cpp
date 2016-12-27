#include "ImageUtil.h"

namespace OIV
{

    ImageUtil::MapConvertKeyToFunc ImageUtil::sConvertionFunction
    {
        // Convert to RGBA
        { IT_BYTE_BGR << 16 | IT_BYTE_RGBA,PixelUtil::BGR24ToRGBA32 }
        ,{ IT_BYTE_RGB << 16 | IT_BYTE_RGBA,PixelUtil::RGB24ToRGBA32 }
        ,{ IT_BYTE_BGRA << 16 | IT_BYTE_RGBA,PixelUtil::BGRA32ToRGBA32 }

    };
}
