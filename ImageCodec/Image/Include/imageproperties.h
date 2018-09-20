#pragma once
#include <cstdint>
#include <limits>
#include "TexelFormat.h"
#include "Buffer.h"


namespace IMCodec
{

	class ImageDescriptor
	{
	public:
        
        ImageDescriptor() = default;
        ImageDescriptor(const ImageDescriptor& properties)
        {
            fProperties = properties.fProperties;
            fMetaData = properties.fMetaData;
            fData = std::move(const_cast<ImageDescriptor&>(properties).fData);
        }
        

        struct MetaData
        {
            double LoadTime = 0.0;
            int exifOrientation = 0;
        };


        struct Properties
        {
            uint32_t Width = std::numeric_limits<uint32_t>::max();
            uint32_t Height = ::std::numeric_limits<uint32_t>::max();
            uint32_t RowPitchInBytes = std::numeric_limits<uint32_t>::max();
            // The texel format of the image after decompression.
            TexelFormat TexelFormatDecompressed = TexelFormat::UNKNOWN;
            // The original texel format of the image.
            TexelFormat TexelFormatStorage = TexelFormat::UNKNOWN;
            uint32_t NumSubImages = std::numeric_limits<uint32_t>::max();
            bool IsInitialized() const
            {
                return !( false 
                    || TexelFormatDecompressed == TexelFormat::UNKNOWN
                    || Width == std::numeric_limits<std::size_t>::max()
                    || Height == std::numeric_limits<std::size_t>::max()
                    || RowPitchInBytes == std::numeric_limits<std::size_t>::max()
                    || NumSubImages == std::numeric_limits<std::size_t>::max());
            }
        };

        Properties fProperties;
        LLUtils::Buffer fData;
        MetaData fMetaData;

		bool IsInitialized() const;

	};
}
