#include "CodecPSDFactory.h"
#include "../Source/CodecPSD.h"

namespace OIV
{
    IImagePlugin* CodecPSDFactory::Create()
    {
        return new CodecPSD();
    }
}
