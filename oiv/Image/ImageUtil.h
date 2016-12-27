#pragma once
#include <unordered_map>
#include "Image.h"
#include "PixelUtil.h"

namespace OIV
{
    

    class ImageUtil
    {
    private:
        
        typedef std::unordered_map<uint32_t, PixelConvertFunc> MapConvertKeyToFunc;

        static MapConvertKeyToFunc sConvertionFunction;
        

    public:
        static ImageSharedPtr ConvertToRGBA(ImageSharedPtr sourceImage)
        {
            ImageSharedPtr convertedImage;

            if (sourceImage->GetImageType() != IT_BYTE_RGBA)
            {
                auto converter = sConvertionFunction.find(sourceImage->GetImageType() << 16 | IT_BYTE_RGBA);
                if (converter != sConvertionFunction.end())
                {
                    uint8_t* dest = nullptr;
                    //TODO: convert without normalization.
                    sourceImage->Normalize();
                    PixelUtil::Convert(converter->second, (uint8_t**)&dest, (uint8_t*)sourceImage->GetBuffer(), sourceImage->GetBitsPerTexel(), sourceImage->GetTotalPixels());
                    ImageProperies properties = sourceImage->GetProperties();
                    properties.Type = IT_BYTE_RGBA;
                    properties.ImageBuffer = dest;
                    properties.RowPitchInBytes = sourceImage->GetRowPitchInTexels() * 4;
                    properties.BitsPerTexel = 32;
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
