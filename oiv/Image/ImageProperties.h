#pragma once
#include <cstdint>
#include <cstdio>

namespace OIV
{
	enum ImageType
	{
		IT_UNKNOWN
		, IT_BYTE_RGBA
		, IT_BYTE_BGR
		, IT_BYTE_BGRA
		, IT_BYTE_RGB
		, IT_BYTE_ARGB
		, IT_BYTE_ABGR
		, IT_FLOAT
		, IT_BYTE_8BIT
	};


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
