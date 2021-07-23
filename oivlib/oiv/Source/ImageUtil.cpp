#include "ImageUtil.h"

namespace IMUtil
{

    ImageUtil::MapConvertKeyToFunc ImageUtil::sConvertionFunction
    {
        // Convert to RGBA
         { ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8,    IMCodec::TexelFormat::I_R8_G8_B8_A8),PixelUtil::BGR24ToRGBA32 }
        ,{ ConvertKey(IMCodec::TexelFormat::I_R8_G8_B8,    IMCodec::TexelFormat::I_R8_G8_B8_A8),PixelUtil::RGB24ToRGBA32 }
        ,{ ConvertKey(IMCodec::TexelFormat::I_B8_G8_R8_A8, IMCodec::TexelFormat::I_R8_G8_B8_A8),PixelUtil::BGRA32ToRGBA32 }
        ,{ ConvertKey(IMCodec::TexelFormat::I_A8,          IMCodec::TexelFormat::I_R8_G8_B8_A8),PixelUtil::A8ToRGBA32 }
        ,{ ConvertKey(IMCodec::TexelFormat::I_R8_G8_B8_A8, IMCodec::TexelFormat::I_B8_G8_R8_A8),PixelUtil::RGBA32ToBGRA32 }
        ,{ ConvertKey(IMCodec::TexelFormat::I_A8_R8_G8_B8, IMCodec::TexelFormat::I_R8_G8_B8_A8),PixelUtil::ARGB32ToRGBA32 }
        ,{ ConvertKey(IMCodec::TexelFormat::I_A8_B8_G8_R8, IMCodec::TexelFormat::I_R8_G8_B8_A8),PixelUtil::ABGR32ToRGBA32 }
        ,{ ConvertKey(IMCodec::TexelFormat::I_A8_B8_G8_R8, IMCodec::TexelFormat::I_B8_G8_R8_A8),PixelUtil::ABGR32ToBGRA32 }
        ,{ ConvertKey(IMCodec::TexelFormat::I_R16_G16_B16, IMCodec::TexelFormat::I_R8_G8_B8_A8),PixelUtil::RGB48ToBGRA32 }
        ,{ ConvertKey(IMCodec::TexelFormat::I_X1,          IMCodec::TexelFormat::I_R8_G8_B8_A8),PixelUtil::A1ToRGBA32 }
        
    };
}
