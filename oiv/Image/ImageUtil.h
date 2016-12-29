#pragma once
#include <unordered_map>
#include "Image.h"
#include "PixelUtil.h"

namespace OIV
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

            ConvertKey(ImageType aSource, ImageType aTarget)
            {
              source = aSource;
              target = aTarget;
            }

            bool operator==(const ConvertKey& rhs) const
            {
                return source == rhs.source && target == rhs.target;
            }

            ImageType source;
            ImageType target;
        };

        typedef std::unordered_map<ConvertKey, PixelConvertFunc, ConvertKey::Hash> MapConvertKeyToFunc;
        static MapConvertKeyToFunc sConvertionFunction;

    public:

        static ImageSharedPtr Convert(ImageSharedPtr sourceImage, ImageType targetPixelFormat)
        {
            ImageSharedPtr convertedImage;

            if (sourceImage->GetImageType() != targetPixelFormat)
            {
                
                auto converter = sConvertionFunction.find(ConvertKey(sourceImage->GetImageType(), targetPixelFormat));

                if (converter != sConvertionFunction.end())
                {
                    uint8_t targetPixelSize = ImageTypeSize(targetPixelFormat);
                    uint8_t* dest = nullptr;

                    //TODO: convert without normalization.
                    sourceImage->Normalize();
                    PixelUtil::Convert(converter->second
                        , &dest
                        , sourceImage->GetBuffer()
                        , targetPixelSize
                        , sourceImage->GetTotalPixels());

                    ImageProperies properties = sourceImage->GetProperties();
                    properties.Type = targetPixelFormat;
                    properties.ImageBuffer = dest;
                    properties.RowPitchInBytes = sourceImage->GetRowPitchInTexels() * (targetPixelSize / 8);
                    properties.BitsPerTexel = targetPixelSize;
                    convertedImage = ImageSharedPtr(new Image(properties, sourceImage->GetLoadTime()));
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
