#pragma once
#include <d3d10.h>
#include <cstdint>

namespace OIV
{
    struct Blob
    {
        Blob(ID3D10Blob* blob)
        {
            size = blob->GetBufferSize();
            buffer = new uint8_t[size];
            memcpy(buffer, blob->GetBufferPointer(), size);
        }
        std::size_t size = 0;
        uint8_t* buffer = nullptr;

        Blob() {}

        ~Blob()
        {
            if (buffer)
                delete[] buffer;
        }

    };
}
