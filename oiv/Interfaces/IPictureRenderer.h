#pragma once
#include "../API/defs.h"
#include <Image.h>


namespace OIV
{
    class IPictureRenderer
    {
    public:
        virtual double Zoom(double percentage,int x, int y) = 0;
        virtual int Pan(double x, double y) = 0;
        virtual int LoadFile(void* buffer, std::size_t size, char* extension, OIV_CMD_LoadFile_Flags flags, ImageHandle& handle) = 0;
        virtual ResultCode LoadRaw(const OIV_CMD_LoadRaw_Request& loadRawRequest, int16_t& handle) = 0;
        virtual ResultCode UnloadFile(const ImageHandle handle) = 0;
        virtual ResultCode DisplayFile(const OIV_CMD_DisplayImage_Request& display_request) = 0;

        virtual ResultCode SetSelectionRect(const OIV_CMD_SetSelectionRect_Request& selectionRect) = 0;

        virtual int Init() = 0;
        virtual int SetParent(std::size_t handle) = 0;
        virtual int Refresh() = 0;

        virtual int GetFileInformation(QryFileInformation& information) = 0;

        virtual IMCodec::Image* GetImage(ImageHandle handle) = 0;
        virtual int SetFilterLevel(OIV_Filter_type filter_level) = 0;
        virtual int GetTexelAtMousePos(int mouseX, int mouseY, double& texelX, double& texelY) = 0;
        virtual int SetTexelGrid(double gridSize) = 0;
        virtual int GetNumTexelsInCanvas(double &x, double &y) = 0;
        virtual int SetClientSize(uint16_t width, uint16_t height) = 0;
        virtual ResultCode AxisAlignTrasnform(const OIV_CMD_AxisAlignedTransform_Request& request) = 0;
        virtual ~IPictureRenderer() {}
        virtual ResultCode SetZoomScrollState(const OIV_CMD_ZoomScrollState_Request* zoom_scroll_state) = 0;
        virtual ResultCode CropImage(const OIV_CMD_CropImage_Request& oiv_cmd_get_pixel_buffer_request, OIV_CMD_CropImage_Response& oiv_cmd_get_pixel_buffer_response) = 0;
        virtual ResultCode WindowToImage(const OIV_CMD_WindowToImage_Request& req, OIV_CMD_WindowToImage_Response& oiv_cmd_window_to_image_response) = 0;
        virtual ResultCode GetPixels(const OIV_CMD_GetPixels_Request& req, OIV_CMD_GetPixels_Response& res) = 0;
        virtual ResultCode ConverFormat(const OIV_CMD_ConvertFormat_Request& req) = 0;
    };
}
