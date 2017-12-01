#pragma once
#include <cstdint>
#include <limits>
#include "TexelFormat.h"


namespace IMCodec
{
	class ImageProperies
	{
	public:
		bool IsInitialized() const;

        uint32_t Width =  std::numeric_limits<uint32_t>::max();
        uint32_t Height = ::std::numeric_limits<uint32_t>::max();
        uint32_t RowPitchInBytes = std::numeric_limits<uint32_t>::max();
		
        // The texel format of the image after decompression.
	    TexelFormat TexelFormatDecompressed = TexelFormat::UNKNOWN;
        // The original texel format of the image.
        TexelFormat TexelFormatStorage = TexelFormat::UNKNOWN;
        uint32_t NumSubImages = std::numeric_limits<uint32_t>::max();
		uint8_t* ImageBuffer = nullptr;
	};

    class ImageData
    {
    public:
        double LoadTime = 0.0;
        int exifOrientation = 0;
    };
}
