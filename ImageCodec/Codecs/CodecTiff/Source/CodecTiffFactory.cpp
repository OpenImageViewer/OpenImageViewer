#include "../Include/CodecTiffFactory.h"
#include "CodecTiff.h"

namespace IMCodec
{
    IImagePlugin* CodecTiffFactory::Create()
    {
        return new CodecTiff();
    }
}
