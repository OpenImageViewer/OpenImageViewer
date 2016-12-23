#pragma once
#include "Image.h"
#include "PixelUtil.h"


namespace OIV
{
    class ImageUtil
    {
    public:
        
        static ImageSharedPtr ConvertToRGBA(ImageSharedPtr sourceImage)
        {
            ImageSharedPtr convertedImage;
            if (sourceImage->GetImageType() != IT_BYTE_RGBA)
            {
                if (sourceImage->GetImageType() == IT_BYTE_RGB)
                {
                    //TODO: convert without normalization.
                    sourceImage->Normalize();
                    uint8_t* dest = nullptr;
                    PixelUtil::RGB24ToRGBA32((uint8_t**)&dest, (uint8_t*)sourceImage->GetBuffer(), sourceImage->GetSizeInMemory());
                    ImageProperies properties = sourceImage->GetProperties();
                    properties.Type = IT_BYTE_RGBA;
                    properties.ImageBuffer = dest;
                    properties.RowPitchInBytes = sourceImage->GetRowPitchInTexels() * 4;
                    properties.BitsPerTexel = 32;
                    convertedImage = ImageSharedPtr(new Image(properties,sourceImage->GetLoadTime()));
                }

                if (sourceImage->GetImageType() == IT_BYTE_BGR)
                {
                    sourceImage->Normalize();
                    uint8_t* dest = nullptr;
                    PixelUtil::BGR24ToRGBA32((uint8_t**)&dest, (uint8_t*)sourceImage->GetBuffer(), sourceImage->GetSizeInMemory());
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
