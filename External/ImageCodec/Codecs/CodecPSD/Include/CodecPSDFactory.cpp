#include "../Include/CodecPSDFactory.h"
#include "CodecPSD.h"

namespace IMCodec
{
    IImagePlugin* CodecPSDFactory::Create()
    {
        return new CodecPSD();
    }
}
