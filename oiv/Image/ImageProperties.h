#pragma once
#include <cstdint>
#include "ImageType.h"

namespace OIV
{
	class ImageProperies
	{
	public:
		ImageProperies();

		bool IsInitialized() const;

		std::size_t Width;
		std::size_t Height;
		std::size_t RowPitchInBytes;
		std::size_t BitsPerTexel;
		ImageType Type;
		std::size_t NumSubImages;
		uint8_t* ImageBuffer;
	};
}
