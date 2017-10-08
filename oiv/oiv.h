#pragma once
#include "Interfaces\IPictureRenderer.h"
#include <ImageLoader.h>
#include "interfaces/IRenderer.h"
#include "ImageManager.h"


namespace OIV
{
    class OIV  : public IPictureRenderer
    {


        public:
#pragma region //-------------IPictureListener implementation------------------
        ResultCode SetZoom(double percentage) override;
        ResultCode SetOffset(uint32_t x, uint32_t y) override;
        ResultCode UnloadFile(const ImageHandle handle) override;
        ResultCode LoadFile(void* buffer, std::size_t size, char* extension , OIV_CMD_LoadFile_Flags flags, ImageHandle& handle) override;
        ResultCode LoadRaw(const OIV_CMD_LoadRaw_Request& loadRawRequest, int16_t& handle) override;
        ResultCode DisplayFile(const OIV_CMD_DisplayImage_Request& display_flags) override;
        ResultCode SetSelectionRect(const OIV_CMD_SetSelectionRect_Request& selectionRect) override;
        ResultCode ConverFormat(const OIV_CMD_ConvertFormat_Request& req) override;
        ResultCode GetPixels(const OIV_CMD_GetPixels_Request& req, OIV_CMD_GetPixels_Response& res) override;
        ResultCode CropImage(const OIV_CMD_CropImage_Request& oiv_cmd_get_pixel_buffer_request, OIV_CMD_CropImage_Response& oiv_cmd_get_pixel_buffer_response) override;
        ResultCode SetColorExposure(const OIV_CMD_ColorExposure_Request& exposure) override;
        ResultCode GetTexelInfo(const OIV_CMD_TexelInfo_Request& texel_request, OIV_CMD_TexelInfo_Response& texelresponse) override;
        int Init() override;
        int SetParent(std::size_t handle) override;
        int Refresh() override;
        
        IMCodec::ImageSharedPtr GetImage(ImageHandle handle) override;
        int SetFilterLevel(OIV_Filter_type filter_level) override;
        ResultCode GetFileInformation(ImageHandle handle, OIV_CMD_QueryImageInfo_Response& information) override;
        int SetTexelGrid(double gridSize) override;
        int SetClientSize(uint16_t width, uint16_t height) override;
        ResultCode AxisAlignTrasnform(const OIV_CMD_AxisAlignedTransform_Request& request) override;
        ResultCode SetZoomScrollState(const OIV_CMD_ZoomScrollState_Request* zoom_scroll_state) override;
#pragma endregion

#pragma region //-------------Private methods------------------
        IRendererSharedPtr CreateBestRenderer();
        bool IsImageDisplayed() const;
        void UpdateGpuParams();
        OIV_AxisAlignedRTransform ResolveExifRotation(unsigned short exifRotation) const;
        IMCodec::ImageSharedPtr ApplyExifRotation(IMCodec::ImageSharedPtr image) const;
        IMCodec::ImageSharedPtr GetDisplayImage() const;
        void RefreshRenderer();
        LLUtils::PointI32 GetClientSize() const;
#pragma endregion

#pragma region //-------------Private member fields------------------

    private:

        std::string fCurrentOpenedFile;
        IMCodec::ImageLoader fImageLoader;
        ImageManager fImageManager;
        IRendererSharedPtr fRenderer = nullptr;
        ViewParameters fViewParams = {};
        std::size_t fParent = 0;
        bool fShowGrid = false;
        IMCodec::ImageSharedPtr fDisplayedImage = nullptr;
        OIV_Filter_type fFilterLevel = OIV_Filter_type::FT_Linear;

        LLUtils::PointI32 fClientSize = LLUtils::PointI32::Zero;
        bool fIsViewDirty = true;
        LLUtils::PointI32 fOffset = 0;
        LLUtils::PointF64 fZoom = 1.0;
#pragma endregion
    };
}
