#pragma once
#include "../OIVImage/OIVHandleImage.h"
#include "../OIVCommands.h"
namespace OIV
{
    
    //Create an RGBA render compatible image, 
    // TODO: accept any render compatible image format.

    class OIVImageHelper
    {
    public:

        //TODO: add convertion parameters in case normalization is needed instead of the rainbow parameter.
        static OIVBaseImageUniquePtr ConvertImage(OIVBaseImageUniquePtr image, OIV_TexelFormat texelFormat, bool useRainbow)
        {
            ImageHandle convertedHandle = ImageHandleNull;
            if (image->GetDescriptor().texelFormat != TF_I_R8_G8_B8_A8)
            {
                if (OIVCommands::ConvertImage(image->GetDescriptor().ImageHandle, texelFormat, useRainbow, convertedHandle) == RC_Success)
                {
                    OIVBaseImageUniquePtr convertedImage = std::make_unique<OIVHandleImage>(convertedHandle);
                    return convertedImage;
                }
                else
                    return nullptr;
            }
            else
            {
                return image;
            }
        }

        static OIVBaseImageUniquePtr GetRendererCompatibleImage(OIVBaseImageUniquePtr image, bool useRainbow)
        {
            ImageHandle convertedHandle = ImageHandleNull;
            if (image->GetDescriptor().texelFormat != TF_I_R8_G8_B8_A8)
            {
                return ConvertImage(image, TF_I_R8_G8_B8_A8, useRainbow);
            }
            else
            {
                return image;
            }
        }
    };
}