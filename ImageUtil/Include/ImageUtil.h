#pragma once
#include <unordered_map>
#include <Image.h>
#include "PixelUtil.h"

namespace IMUtil
{
    class ImageUtil
    {
    private:
        struct ConvertKey
        {
            
            struct Hash
            {
                std::size_t operator()(const ConvertKey& key) const
                {
                    return key.source << 16 | key.target;
                }
            };

            ConvertKey(IMCodec::ImageType aSource, IMCodec::ImageType aTarget)
            {
              source = aSource;
              target = aTarget;
            }

            bool operator==(const ConvertKey& rhs) const
            {
                return source == rhs.source && target == rhs.target;
            }

            IMCodec::ImageType source;
            IMCodec::ImageType target;
        };

        typedef std::unordered_map<ConvertKey, PixelConvertFunc, ConvertKey::Hash> MapConvertKeyToFunc;
        static MapConvertKeyToFunc sConvertionFunction;

    public:

        static IMCodec::ImageSharedPtr Transform(AxisAlignedRTransform transform, IMCodec::ImageSharedPtr image)
        {
            using namespace std;
            using namespace IMCodec;

            if (image->GetIsByteAligned() == false)
                throw std::logic_error("OIV::Image::Transom works only with byte aligned image formats");

            if (transform != AAT_None)
            {
                uint8_t* dest = new uint8_t[image->GetTotalSizeOfImageTexels()];

                const size_t MegaBytesPerThread = 6;
                static const uint8_t MaxGlobalThrads = 32;
                static uint8_t maxThreads = static_cast<uint8_t>(
                    min(static_cast<unsigned int>(MaxGlobalThrads), max(1u, thread::hardware_concurrency() - 1)));
                static thread threads[MaxGlobalThrads];


                const size_t bytesPerThread = MegaBytesPerThread * 1024 * 1024;
                const uint8_t totalThreads = std::min(maxThreads, static_cast<uint8_t>(image->GetTotalSizeOfImageTexels() / bytesPerThread));
                PixelUtil::TransformTexelsInfo descTemplate;
                descTemplate.transform = transform;
                descTemplate.dstBuffer = dest;
                descTemplate.srcBuffer = image->GetBuffer();
                descTemplate.width = image->GetWidth();
                descTemplate.height = image->GetHeight();
                descTemplate.bytesPerTexel = image->GetBytesPerTexel();
                descTemplate.srcRowPitch = image->GetRowPitchInBytes();
                descTemplate.startCol = 0;
                descTemplate.endCol = image->GetWidth();
                descTemplate.startRow = 0;
                descTemplate.endRow = image->GetHeight();

                if (totalThreads > 0)
                {
                    size_t rowsPerThread = image->GetHeight() / totalThreads;
                    for (uint8_t threadNum = 0; threadNum < totalThreads; threadNum++)
                    {

                        threads[threadNum] = std::thread
                        (
                            [&descTemplate, rowsPerThread, threadNum]()
                        {
                            PixelUtil::TransformTexelsInfo desc = descTemplate;
                            desc.startRow = rowsPerThread * threadNum;
                            desc.endRow = rowsPerThread * (threadNum + 1);
                            PixelUtil::TransformTexels(desc);
                        }
                        );
                    }

                    PixelUtil::TransformTexelsInfo desc = descTemplate;
                    desc.startRow = rowsPerThread * totalThreads;
                    desc.endRow = image->GetHeight();

                    PixelUtil::TransformTexels(desc);

                    for (uint8_t i = 0; i < totalThreads; i++)
                        threads[i].join();
                }
                else
                {
                    // single (main) thread implementation
                    PixelUtil::TransformTexels(descTemplate);
                }

                ImageProperies transformedProperties = image->GetProperties();

                if (transform == AAT_Rotate90CW || transform == AAT_Rotate90CCW)
                    swap(transformedProperties.Height, transformedProperties.Width);

                transformedProperties.RowPitchInBytes = transformedProperties.Width * image->GetBytesPerTexel();
                transformedProperties.ImageBuffer = dest;
                return ImageSharedPtr(new Image(transformedProperties, image->GetLoadTime()));
            }
            else
                return image;
            
        }
        
        static IMCodec::ImageSharedPtr Normalize(IMCodec::ImageSharedPtr image)
        {
            using namespace IMCodec;

            if (image->GetIsByteAligned() == false)
                throw std::logic_error("Can not normalize a non byte aligned pixel format");

            ImageProperies normalizedImageProperties;

            if (image->GetIsRowPitchNormalized() == true)
                throw std::logic_error("Image already normalized");


            const uint8_t* oldBuffer = image->GetBuffer();
            normalizedImageProperties = image->GetProperties();
            std::size_t targetRowPitch = image->GetBytesPerRowOfPixels();
            uint8_t* newBuffer = new uint8_t[image->GetTotalSizeOfImageTexels()];
            normalizedImageProperties.ImageBuffer = newBuffer;
            for (std::size_t y = 0; y < image->GetHeight(); y++)
                for (std::size_t x = 0; x < targetRowPitch; x++)
                {
                    std::size_t srcIndex = y * image->GetRowPitchInBytes() + x;
                    std::size_t dstIndex = y * targetRowPitch + x;

                    newBuffer[dstIndex] = oldBuffer[srcIndex];
                }

            normalizedImageProperties.ImageBuffer = newBuffer;
            normalizedImageProperties.RowPitchInBytes = targetRowPitch;

            
            return ImageSharedPtr(new Image(normalizedImageProperties, image->GetLoadTime()));
        }

        static IMCodec::ImageSharedPtr Convert(IMCodec::ImageSharedPtr sourceImage, IMCodec::ImageType targetPixelFormat)
        {
            using namespace IMCodec;
            ImageSharedPtr convertedImage;

            if (sourceImage->GetImageType() != targetPixelFormat)
            {
                
                auto converter = sConvertionFunction.find(ConvertKey(sourceImage->GetImageType(), targetPixelFormat));

                if (converter != sConvertionFunction.end())
                {
                    uint8_t targetPixelSize = ImageTypeSize(targetPixelFormat);
                    uint8_t* dest = nullptr;
                    
                    //TODO: convert without normalization.
                    convertedImage = sourceImage->GetIsRowPitchNormalized() == true ? sourceImage : Normalize(sourceImage);
                        
                    PixelUtil::Convert(converter->second
                        , &dest
                        , convertedImage->GetBuffer()
                        , targetPixelSize
                        , convertedImage->GetTotalPixels());

                    ImageProperies properties = convertedImage->GetProperties();
                    properties.Type = targetPixelFormat;
                    properties.ImageBuffer = dest;
                    properties.RowPitchInBytes = convertedImage->GetRowPitchInTexels() * (targetPixelSize / 8);
                    properties.BitsPerTexel = targetPixelSize;
                    return ImageSharedPtr(new Image(properties, convertedImage->GetLoadTime()));
                }
            }
            else
            { // No need to convert, return source image.
                convertedImage = sourceImage;

            }
            return convertedImage;
        }
    };
}
