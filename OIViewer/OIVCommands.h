#pragma once
#include "API/defs.h"
#include "TestApp.h"
#include <Rect.h>

namespace OIV
{
    class OIVCommands
    {
    public:

        static OIV_RECT_I ToOIVRect(const LLUtils::RectI32& rect)
        {
            return{ rect.p0.x,rect.p0.y ,rect.p1.x,rect.p1.y };
        }

        template <class T, class U>
        static ResultCode ExecuteCommand(CommandExecute command, T* request, U* response)
        {
            return OIV_Execute(command, sizeof(T), request, sizeof(U), response);
        }

        static ResultCode Refresh()
        {
            return ExecuteCommand(CommandExecute::CE_Refresh, &CmdNull(), &CmdNull());
        }

        static ResultCode ClearImage()
        {
            OIV_CMD_DisplayImage_Request displayRequest = {};

            displayRequest.handle = ImageNullHandle;
            return ExecuteCommand(CommandExecute::OIV_CMD_DisplayImage, &displayRequest, &CmdNull());
        }

        static ResultCode UnloadImage(ImageHandle handle)
        {
            if (handle != ImageNullHandle)
            {
                OIV_CMD_UnloadFile_Request unloadRequest = {};
                unloadRequest.handle = handle;
                return ExecuteCommand(CommandExecute::OIV_CMD_UnloadFile, &unloadRequest, &CmdNull());
            }
            else return RC_InvalidHandle;
        }

        static ResultCode TransformImage(ImageHandle handle, OIV_AxisAlignedRTransform transform)
        {
            OIV_CMD_AxisAlignedTransform_Request request = {};
            request.handle = handle;
            request.transform = transform;

            return ExecuteCommand(CommandExecute::OIV_CMD_AxisAlignedTransform, &request, &CmdNull());
        }

        static ResultCode ConvertImage(ImageHandle handle, OIV_TexelFormat desiredTexelFormat)
        {
            OIV_CMD_ConvertFormat_Request request = {};
            request.handle = handle;
            request.format = desiredTexelFormat;

            return ExecuteCommand(CommandExecute::OIV_CMD_ConvertFormat, &request, &CmdNull());
        }


        static ResultCode CropImage(ImageHandle sourceImage, const LLUtils::RectI32& rect, ImageHandle& croppedHandle)
        {
            OIV_CMD_CropImage_Request requestCropImage;
            OIV_CMD_CropImage_Response responseCropImage;

            requestCropImage.rect = ToOIVRect(rect);
            requestCropImage.imageHandle = sourceImage;

            // 1. create a new cropped image 
            ResultCode result = ExecuteCommand(OIV_CMD_CropImage, &requestCropImage, &responseCropImage);
            croppedHandle = (result == RC_Success ? responseCropImage.imageHandle : ImageNullHandle);
            return result;
        }

        static ResultCode DisplayImage(ImageHandle image_handle
            , OIV_CMD_DisplayImage_Flags displayFlags
            , OIV_PROP_Normalize_Mode normalizationFlags = OIV_PROP_Normalize_Mode::NM_Default
        )
        {
            OIV_CMD_DisplayImage_Request displayRequest = {};

            displayRequest.handle = image_handle;
            displayRequest.displayFlags = displayFlags;
            displayRequest.normalizeMode = normalizationFlags;

            return ExecuteCommand(CommandExecute::OIV_CMD_DisplayImage, &displayRequest, &CmdNull());
        }
    };
}