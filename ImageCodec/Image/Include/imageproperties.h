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

        std::size_t Width =  std::numeric_limits<std::size_t>::max();
        std::size_t Height = ::std::numeric_limits<std::size_t>::max();
        std::size_t RowPitchInBytes = std::numeric_limits<std::size_t>::max();
		
        // The texel format of the image after decompression.
	    TexelFormat TexelFormatDecompressed = TF_UNKNOWN;
        // The original texel format of the image.
        TexelFormat TexelFormatStorage = TF_UNKNOWN;
        std::size_t NumSubImages = std::numeric_limits<std::size_t>::max();
		uint8_t* ImageBuffer = nullptr;
	};

    class ImageData
    {
    public:
        double LoadTime = 0.0;
        int exifOrientation = 0;
    };
}
