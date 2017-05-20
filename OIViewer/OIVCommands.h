#pragma once
#include "API/defs.h"
#include "TestApp.h"

namespace OIV
{
    class OIVCommands
    {
    public:
        template <class T, class U>
        static ResultCode OIVCommands::ExecuteCommand(CommandExecute command, T* request, U* response)
        {
            return OIV_Execute(command, sizeof(T), request, sizeof(U), response);
        }

        static ResultCode OIVCommands::UnloadImage(ImageHandle handle)
        {
            OIV_CMD_UnloadFile_Request unloadRequest = {};
            unloadRequest.handle = handle;
            return ExecuteCommand(CommandExecute::OIV_CMD_UnloadFile, &unloadRequest, &CmdNull());
        }

        static ResultCode OIVCommands::TransformImage(ImageHandle handle, OIV_AxisAlignedRTransform transform)
        {
            OIV_CMD_AxisAlignedTransform_Request request = {};
            request.handle = handle;
            request.transform = transform;
            
            return ExecuteCommand(CommandExecute::OIV_CMD_AxisAlignedTransform, &request, &CmdNull());
        }

        static ResultCode OIVCommands::ConvertImage(ImageHandle handle, OIV_TexelFormat desiredTexelFormat)
        {
            OIV_CMD_ConvertFormat_Request request = {};
            request.handle = handle;
            request.format = desiredTexelFormat;

            return ExecuteCommand(CommandExecute::OIV_CMD_ConvertFormat, &request, &CmdNull());
        }
    };

}
