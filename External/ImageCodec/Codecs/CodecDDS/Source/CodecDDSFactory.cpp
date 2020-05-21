#include "../Include/CodecDDSFactory.h"
#include "CodecDDS.h"

namespace IMCodec
{
    IImagePlugin* CodecDDSFactory::Create()
    {
        return new CodecDDS();
    }
}
