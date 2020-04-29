#include "CodecPSDFactory.h"
#include "../Source/CodecPSD.h"

namespace IMCodec
{
    IImagePlugin* CodecPSDFactory::Create()
    {
        return new CodecPSD();
    }
}
