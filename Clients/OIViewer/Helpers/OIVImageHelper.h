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
            if (image->GetDescriptor().texelFormat != texelFormat)
            {
                if (OIVCommands::ConvertImage(image->GetDescriptor().ImageHandle, texelFormat, useRainbow, convertedHandle) == RC_Success)
                {
                    OIVBaseImageUniquePtr convertedImage = std::make_unique<OIVHandleImage>(convertedHandle);
                    return convertedImage;
                }
                else
                {
                    LL_EXCEPTION(LLUtils::Exception::ErrorCode::RuntimeError, "Unable to convert image");
                }

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
        static OIVBaseImageUniquePtr ResampleImage(OIVBaseImageUniquePtr image, LLUtils::PointI32 scale)
        {
            OIV_CMD_Resample_Request req{};
            req.imageHandle = image->GetDescriptor().ImageHandle;
            req.size = static_cast<LLUtils::PointI32>(scale);
            OIV_CMD_Resample_Response res;

            if (OIVCommands::ExecuteCommand(OIV_CMD_ResampleImage, &req, &res) == RC_Success)
            {
                OIVBaseImageUniquePtr resampledImage = std::make_unique<OIVHandleImage>(res.imageHandle);
                return resampledImage;
            }
            else
            {
                return OIVBaseImageUniquePtr();
            }
        }
    };
}