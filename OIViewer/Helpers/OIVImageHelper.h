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
        static OIVBaseImageUniquePtr GetRendererCompatibleImage(OIVBaseImageUniquePtr image, bool useRainbow)
        {
            ImageHandle convertedHandle = ImageHandleNull;
            if (image->GetDescriptor().texelFormat != TF_I_R8_G8_B8_A8)
            {
                if (OIVCommands::ConvertImage(image->GetDescriptor().ImageHandle, OIV_TexelFormat::TF_I_R8_G8_B8_A8, useRainbow, convertedHandle) == RC_Success)
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
    };
}