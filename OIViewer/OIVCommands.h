#pragma once
#include "API/defs.h"
#include "API/functions.h"
#include <Rect.h>

namespace OIV
{
    class OIVCommands
    {
    public:

        static OIV_RECT_I ToOIVRect(const LLUtils::RectI32& rect)
        {
            LLUtils::RectI32::Point_Type topleft = rect.GetCorner(LLUtils::Corner::TopLeft);
            LLUtils::RectI32::Point_Type bottomRight = rect.GetCorner(LLUtils::Corner::BottomRight);
            return{ topleft.x, topleft.y, bottomRight.x, bottomRight.y};
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


        static ResultCode GetKnownFileTypes(std::string& o_fileTypes)
        {

            OIV_CMD_GetKnownFileTypes_Response res = {};

            ResultCode rc = RC_Success;

            if ((rc = ExecuteCommand(CommandExecute::OIV_CMD_GetKnownFileTypes, &CmdNull(), &res)) == RC_Success)
            {
                res.knownFileTypes = new char[res.bufferSize];
                if ((rc = ExecuteCommand(CommandExecute::OIV_CMD_GetKnownFileTypes, &CmdNull(), &res)) == RC_Success)
                    o_fileTypes = res.knownFileTypes;

                delete[] res.knownFileTypes;
            }
            return rc;
            
        }


        static ResultCode ClearImage()
        {
            OIV_CMD_DisplayImage_Request displayRequest = {};

            displayRequest.handle = ImageHandleNull;
            return ExecuteCommand(CommandExecute::OIV_CMD_DisplayImage, &displayRequest, &CmdNull());
        }

        static ResultCode UnloadImage(ImageHandle handle)
        {
            if (handle != ImageHandleNull)
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
        

        static ResultCode OIVCommands::CropImage(ImageHandle sourceImage, const LLUtils::RectI32& rect, ImageHandle& croppedHandle)
        {
            OIV_CMD_CropImage_Request requestCropImage;
            OIV_CMD_CropImage_Response responseCropImage;

            requestCropImage.rect = ToOIVRect(rect);
            requestCropImage.imageHandle = sourceImage;

            // 1. create a new cropped image 
            ResultCode result = ExecuteCommand(OIV_CMD_CropImage, &requestCropImage, &responseCropImage);
            croppedHandle = (result == RC_Success ? responseCropImage.imageHandle : ImageHandleNull);
            return result;
        }

        static ResultCode OIVCommands::DisplayImage(ImageHandle image_handle
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

        static void SetSelectionRect(const LLUtils::RectI32& rect)
        {
            OIV_CMD_SetSelectionRect_Request request;
            request.rect.x0 = rect.GetCorner(LLUtils::TopLeft).x;
            request.rect.y0 = rect.GetCorner(LLUtils::TopLeft).y;
            request.rect.x1 = rect.GetCorner(LLUtils::BottomRight).x;
            request.rect.y1 = rect.GetCorner(LLUtils::BottomRight).y;
            ExecuteCommand(CommandExecute::OIV_CMD_SetSelectionRect, &request, &CmdNull());
        }

        static void CancelSelectionRect()
        {
            OIV_CMD_SetSelectionRect_Request request = { -1,-1,-1,-1 };
            ExecuteCommand(CommandExecute::OIV_CMD_SetSelectionRect, &request, &CmdNull());
        }
    };
}